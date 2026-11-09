/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "SMSampler.h"
#include <ifaddrs.h>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <net/if.h>
#include <pwd.h>
#include <sys/proc_info.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <unistd.h>

@interface SMProcess ()
@property pid_t pid, parentPID;
@property (copy) NSString *name, *path, *user;
@property double cpuPercent, cpuTime, readRate, writeRate;
@property uint64_t memory, bytesRead, bytesWritten, cpuNanoseconds;
@property int threads;
@property (copy) NSDate *startDate;
@end

@implementation SMProcess
@end

@interface SMInterface ()
@property (copy) NSString *name;
@property uint64_t bytesIn, bytesOut;
@property double inRate, outRate;
@end

@implementation SMInterface
@end

static NSString *userName(uid_t uid)
{
    static NSMutableDictionary *names;
    NSNumber *key = @(uid);

    if (names == nil)
        names = [NSMutableDictionary dictionary];
    if (names[key] == nil) {
        struct passwd *pw = getpwuid(uid);
        names[key] = pw != NULL ? @(pw->pw_name) : [key stringValue];
    }
    return names[key];
}

@implementation SMSampler {
    NSDictionary<NSNumber *, SMProcess *> *_previous;
    NSDictionary<NSString *, SMInterface *> *_previousInterfaces;
    uint64_t _lastTicks[CPU_STATE_MAX];
    uint64_t _lastSample;      /* nanoseconds, monotonic */
}

- (instancetype)init
{
    if ((self = [super init]) != nil) {
        _processors = (int)sysconf(_SC_NPROCESSORS_ONLN);
        size_t size = sizeof(_physicalMemory);
        sysctlbyname("hw.memsize", &_physicalMemory, &size, NULL, 0);
        _processes = @[];
        _interfaces = @[];
    }
    return self;
}

static uint64_t now(void)
{
    return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}

- (void)sampleProcessesSince:(double)seconds
{
    int count = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0) / (int)sizeof(pid_t) + 32;
    pid_t *pids = calloc((size_t)count, sizeof(pid_t));
    NSMutableArray *processes = [NSMutableArray array];
    NSMutableDictionary *byPID = [NSMutableDictionary dictionary];
    double reads = 0, writes = 0;

    count = proc_listpids(PROC_ALL_PIDS, 0, pids, count * (int)sizeof(pid_t)) / (int)sizeof(pid_t);
    for (int i = 0; i < count; i++) {
        pid_t pid = pids[i];
        struct proc_bsdinfo bsd;
        struct proc_taskinfo task;
        struct rusage_info_v2 usage;
        char name[2 * MAXCOMLEN + 1] = "", path[PROC_PIDPATHINFO_MAXSIZE] = "";

        if (pid <= 0 || proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bsd, sizeof(bsd)) != sizeof(bsd))
            continue;
        SMProcess *p = [SMProcess new];
        p.pid = pid;
        p.parentPID = (pid_t)bsd.pbi_ppid;
        proc_name(pid, name, sizeof(name));
        proc_pidpath(pid, path, sizeof(path));
        p.name = name[0] ? @(name) : @(bsd.pbi_comm);
        p.path = @(path);
        p.user = userName(bsd.pbi_uid);
        p.startDate = [NSDate dateWithTimeIntervalSince1970:bsd.pbi_start_tvsec + bsd.pbi_start_tvusec / 1e6];
        if (proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &task, sizeof(task)) == sizeof(task)) {
            p.memory = task.pti_resident_size;
            p.threads = task.pti_threadnum;
            p.cpuNanoseconds = task.pti_total_user + task.pti_total_system;
            p.cpuTime = p.cpuNanoseconds / 1e9;
        }
        if (proc_pid_rusage(pid, RUSAGE_INFO_V2, (rusage_info_t *)&usage) == 0) {
            p.bytesRead = usage.ri_diskio_bytesread;
            p.bytesWritten = usage.ri_diskio_byteswritten;
        }
        SMProcess *before = _previous[@(pid)];
        if (before != nil && seconds > 0) {
            if (p.cpuNanoseconds >= before.cpuNanoseconds)
                p.cpuPercent = 100.0 * (p.cpuNanoseconds - before.cpuNanoseconds) / (seconds * 1e9);
            if (p.bytesRead >= before.bytesRead)
                p.readRate = (p.bytesRead - before.bytesRead) / seconds;
            if (p.bytesWritten >= before.bytesWritten)
                p.writeRate = (p.bytesWritten - before.bytesWritten) / seconds;
        }
        reads += p.readRate;
        writes += p.writeRate;
        [processes addObject:p];
        byPID[@(pid)] = p;
    }
    free(pids);
    _processes = [processes copy];
    _previous = [byPID copy];
    _readRate = reads;
    _writeRate = writes;
}

- (void)sampleSystemSince:(double)seconds
{
    host_cpu_load_info_data_t cpu;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;

    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpu, &count) == KERN_SUCCESS) {
        uint64_t d[CPU_STATE_MAX], total = 0;
        for (int s = 0; s < CPU_STATE_MAX; s++) {
            d[s] = cpu.cpu_ticks[s] - _lastTicks[s];
            total += d[s];
            _lastTicks[s] = cpu.cpu_ticks[s];
        }
        if (total > 0) {
            _userPercent = 100.0 * (d[CPU_STATE_USER] + d[CPU_STATE_NICE]) / total;
            _systemPercent = 100.0 * d[CPU_STATE_SYSTEM] / total;
            _idlePercent = 100.0 * d[CPU_STATE_IDLE] / total;
        }
    }

    vm_statistics64_data_t vm;
    count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm, &count) == KERN_SUCCESS) {
        uint64_t page = (uint64_t)vm_kernel_page_size;
        _freeMemory = (uint64_t)vm.free_count * page;
        _wiredMemory = (uint64_t)vm.wire_count * page;
        _usedMemory = _physicalMemory > _freeMemory ? _physicalMemory - _freeMemory : 0;
    }

    struct ifaddrs *list = NULL;
    NSMutableArray *interfaces = [NSMutableArray array];
    NSMutableDictionary *byName = [NSMutableDictionary dictionary];
    double in = 0, out = 0;
    if (getifaddrs(&list) == 0) {
        for (struct ifaddrs *a = list; a != NULL; a = a->ifa_next) {
            if (a->ifa_addr == NULL || a->ifa_addr->sa_family != AF_LINK || a->ifa_data == NULL)
                continue;
            struct if_data *data = a->ifa_data;
            SMInterface *i = [SMInterface new];
            i.name = @(a->ifa_name);
            i.bytesIn = data->ifi_ibytes;
            i.bytesOut = data->ifi_obytes;
            SMInterface *before = _previousInterfaces[i.name];
            if (before != nil && seconds > 0) {
                if (i.bytesIn >= before.bytesIn)
                    i.inRate = (i.bytesIn - before.bytesIn) / seconds;
                if (i.bytesOut >= before.bytesOut)
                    i.outRate = (i.bytesOut - before.bytesOut) / seconds;
            }
            if (!(a->ifa_flags & IFF_LOOPBACK)) {
                in += i.inRate;
                out += i.outRate;
            }
            [interfaces addObject:i];
            byName[i.name] = i;
        }
        freeifaddrs(list);
    }
    _interfaces = [interfaces copy];
    _previousInterfaces = [byName copy];
    _inRate = in;
    _outRate = out;
}

- (void)sample
{
    uint64_t t = now();
    double seconds = _lastSample ? (t - _lastSample) / 1e9 : 0;

    [self sampleProcessesSince:seconds];
    [self sampleSystemSince:seconds];
    _lastSample = t;
}

- (SMProcess *)processWithPID:(pid_t)pid
{
    return _previous[@(pid)];
}

@end
