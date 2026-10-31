# Minkar — Progress at a glance

The roadmap's state as diagrams; [ROADMAP.md](ROADMAP.md) is the source of
truth and every change there is mirrored here. Spanish:
[Progress.es.md](Progress.es.md). Last updated 2026-10-31.

Colours: green done, yellow partial or in progress, grey not started.

## Critical path and gates

```mermaid
flowchart LR
    P0["Phase 0<br/>Build, toolchain, CI<br/><b>M0 met</b>"]:::done
    P1["Phase 1<br/>Darwin boot<br/><b>M1: PC 2 missing</b>"]:::partial
    P2["Phase 2<br/>Base userland<br/><b>M2 met, closed</b>"]:::done
    P3["Phase 3<br/>Frameworks and Swift<br/><b>M3</b>"]:::partial
    P4["Phase 4<br/>Graphics, WindowServer<br/><b>M4</b>"]:::partial
    P5["Phase 5<br/>Gienah desktop<br/><b>M5</b>"]:::todo
    P6["Phase 6<br/>Apps and distribution<br/><b>M6 = Minkar 19.6.0</b>"]:::todo
    P7["Phase 7<br/>Binary compatibility, arm64<br/><b>M7</b>"]:::todo
    PERF["Minkar 21.0<br/>API 21 + performance"]:::todo
    GPU["GPU track<br/>G1–G5"]:::todo

    P0 --> P1 --> P2 --> P3 --> P4 --> P5 --> P6
    P6 -.-> P7
    P6 -.-> PERF
    GPU -.-> PERF
    P4 -.-> GPU

    classDef done fill:#2e7d32,stroke:#1b5e20,color:#fff
    classDef partial fill:#f9a825,stroke:#f57f17,color:#000
    classDef todo fill:#9e9e9e,stroke:#616161,color:#fff
```

## Phase 3: libSystem and Foundation

```mermaid
flowchart TB
    subgraph LS["libSystem"]
        L1["Reference apps' imports<br/>308 of 308"]:::done
        L2["os_log to syslogd<br/>os_activity"]:::done
        L3["@available<br/>_availability_version_check"]:::done
        L4["launch_activate_socket"]:::done
        L5["Noisy stubs<br/>mechanism + 887 sites"]:::partial
        L6["logd"]:::todo
        L7["dyld shared cache"]:::todo
    end
    subgraph FW["Frameworks"]
        F1["Foundation audit<br/>NSProcessInfo done"]:::partial
        F2["NSURLSession"]:::todo
        F3["CF-Lite to API 21<br/>CFArray/CFDictionary bridging"]:::partial
        F4["Security, LaunchServices"]:::todo
    end
    subgraph SW["Swift 6"]
        S1["Prototype 1: swiftc"]:::todo
        S2["Prototype 2: libswiftCore"]:::todo
        S3["Overlays, Foundation overlay"]:::todo
    end
    S1 --> S2 --> S3
    F4 --> F2

    classDef done fill:#2e7d32,stroke:#1b5e20,color:#fff
    classDef partial fill:#f9a825,stroke:#f57f17,color:#000
    classDef todo fill:#9e9e9e,stroke:#616161,color:#fff
```

## Graphics stack: components by state

Each box says the sequence step (below) that closes it.

```mermaid
flowchart TB
    subgraph APPS["Apps"]
        AK["AppKit (Cocotron)<br/>step 7"]:::partial
    end
    subgraph TEXT["Text and images"]
        CT["Core Text on<br/>HarfBuzz/FreeType<br/>step 6"]:::todo
        IMG["giflib, libwebp<br/>step 6"]:::todo
        CS["ColorSync on<br/>Little CMS<br/>step 6"]:::todo
    end
    subgraph DRAW["Drawing and compositing"]
        CG["Core Graphics<br/>with FreeType"]:::done
        LIBS["pixman, libpng,<br/>libjpeg, libtiff"]:::done
        QC["QuartzCore, Onyx2D"]:::partial
        CA["Core Animation compositor<br/>(GPU first)<br/>step 5"]:::todo
        IOS["IOSurface<br/>step 4"]:::todo
    end
    subgraph SERVER["Window server"]
        WS["WindowServer<br/>builds, never run<br/>step 2"]:::partial
    end
    subgraph INPUT["Input"]
        HIDH["IOKit/hidsystem<br/>headers"]:::done
        USB["xHCI + HID<br/>(MinkarUSB)"]:::done
        KBD["PS/2 keyboard<br/>reaches the console<br/>step 1"]:::partial
        MOUSE["ApplePS2Mouse<br/>step 1"]:::todo
        HIDS["IOHIDSystem event<br/>delivery<br/>step 1"]:::todo
    end
    subgraph DISPLAY["Display"]
        GOP["GOP console<br/>(IOGOPFramebuffer)"]:::done
        VGPU["virtio-gpu 2D<br/>step 3"]:::todo
        GPUD["GPU driver, Mesa,<br/>Metal (G1–G5)"]:::todo
    end

    AK --> CT & QC
    CT --> CG
    QC --> CA --> IOS
    CA --> WS
    WS --> GOP & VGPU & GPUD
    WS --> HIDS
    HIDS --> KBD & MOUSE & USB
    CG --> LIBS
    IMG --> CG
    CS --> CG

    classDef done fill:#2e7d32,stroke:#1b5e20,color:#fff
    classDef partial fill:#f9a825,stroke:#f57f17,color:#000
    classDef todo fill:#9e9e9e,stroke:#616161,color:#fff
```

## Graphics stack: development sequence

```mermaid
flowchart LR
    S1["1. IOHIDSystem events<br/>and the PS/2 mouse"]:::next
    S2["2. WindowServer<br/>on the GOP framebuffer"]:::todo
    S3["3. virtio-gpu 2D<br/>(FreeBSD, BSD-2)"]:::todo
    S4["4. IOSurface<br/>(Mach shared memory)"]:::todo
    S5["5. Core Animation compositor<br/>(Vulkan; pixman for bring-up)"]:::todo
    S6["6. HarfBuzz/FreeType, giflib,<br/>libwebp, Little CMS"]:::todo
    S7["7. Modern AppKit,<br/>layer-backed views"]:::todo
    S1 --> S2 --> S3 --> S4 --> S5 --> S6 --> S7

    G1["G1 GPU family<br/>and KMS prototype"]:::todo
    G2["G2–G5 kernel driver, Mesa<br/>(lavapipe/llvmpipe in VMs),<br/>Metal (darling-metal, Indium)"]:::todo
    G1 --> G2
    S4 -.-> G2
    S5 -.-> G2

    classDef next fill:#1565c0,stroke:#0d47a1,color:#fff
    classDef todo fill:#9e9e9e,stroke:#616161,color:#fff
```

Step 1 is next (blue): without input, WindowServer cannot close M4. G1 starts
as soon as the GPU family is decided; the dashed arrows mark what the GPU
track reuses from steps 4 and 5.
