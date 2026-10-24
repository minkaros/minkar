# libarchive (Tim Kientzle and others, BSD-2: COPYING), from FreeBSD's
# contrib/libarchive in git history (src/), for bsdtar. Static library.
# config_ravynos.h is FreeBSD's config adjusted for Darwin: no ACLs or
# extended attributes, no OpenSSL, xar or zstd; zlib, bzip2 and xz; digests
# from our libmd.

SUBDIR =
MK_WERROR = no
LIB =	archive
.PATH: ${.CURDIR}/src/libarchive

SRCS =	archive_acl.c archive_blake2sp_ref.c archive_blake2s_ref.c archive_check_magic.c \
	archive_cmdline.c archive_cryptor.c archive_digest.c archive_entry.c \
	archive_entry_copy_stat.c archive_entry_link_resolver.c archive_entry_sparse.c archive_entry_stat.c \
	archive_entry_strmode.c archive_entry_xattr.c archive_hmac.c archive_match.c \
	archive_options.c archive_pack_dev.c archive_parse_date.c archive_pathmatch.c \
	archive_ppmd7.c archive_ppmd8.c archive_random.c archive_rb.c \
	archive_read.c archive_read_add_passphrase.c archive_read_append_filter.c archive_read_data_into_fd.c \
	archive_read_disk_entry_from_file.c archive_read_disk_posix.c archive_read_disk_set_standard_lookup.c archive_read_extract.c \
	archive_read_extract2.c archive_read_open_fd.c archive_read_open_file.c archive_read_open_filename.c \
	archive_read_open_memory.c archive_read_set_format.c archive_read_set_options.c archive_read_support_filter_all.c \
	archive_read_support_filter_bzip2.c archive_read_support_filter_by_code.c archive_read_support_filter_compress.c archive_read_support_filter_gzip.c \
	archive_read_support_filter_grzip.c archive_read_support_filter_lrzip.c archive_read_support_filter_lz4.c archive_read_support_filter_lzop.c \
	archive_read_support_filter_none.c archive_read_support_filter_program.c archive_read_support_filter_rpm.c archive_read_support_filter_uu.c \
	archive_read_support_filter_xz.c archive_read_support_filter_zstd.c archive_read_support_format_7zip.c archive_read_support_format_all.c \
	archive_read_support_format_ar.c archive_read_support_format_by_code.c archive_read_support_format_cab.c archive_read_support_format_cpio.c \
	archive_read_support_format_empty.c archive_read_support_format_iso9660.c archive_read_support_format_lha.c archive_read_support_format_mtree.c \
	archive_read_support_format_rar.c archive_read_support_format_rar5.c archive_read_support_format_raw.c archive_read_support_format_tar.c \
	archive_read_support_format_warc.c archive_read_support_format_xar.c archive_read_support_format_zip.c archive_string.c \
	archive_string_sprintf.c archive_time.c archive_util.c archive_version_details.c \
	archive_virtual.c archive_write.c archive_write_add_filter.c archive_write_disk_set_standard_lookup.c \
	archive_write_disk_posix.c archive_write_open_fd.c archive_write_open_file.c archive_write_open_filename.c \
	archive_write_open_memory.c archive_write_add_filter_b64encode.c archive_write_add_filter_by_name.c archive_write_add_filter_bzip2.c \
	archive_write_add_filter_compress.c archive_write_add_filter_grzip.c archive_write_add_filter_gzip.c archive_write_add_filter_lrzip.c \
	archive_write_add_filter_lz4.c archive_write_add_filter_lzop.c archive_write_add_filter_none.c archive_write_add_filter_program.c \
	archive_write_add_filter_uuencode.c archive_write_add_filter_xz.c archive_write_add_filter_zstd.c archive_write_set_format.c \
	archive_write_set_format_7zip.c archive_write_set_format_ar.c archive_write_set_format_by_name.c archive_write_set_format_cpio.c \
	archive_write_set_format_cpio_binary.c archive_write_set_format_cpio_newc.c archive_write_set_format_cpio_odc.c archive_write_set_format_filter_by_ext.c \
	archive_write_set_format_gnutar.c archive_write_set_format_iso9660.c archive_write_set_format_mtree.c archive_write_set_format_pax.c \
	archive_write_set_format_raw.c archive_write_set_format_shar.c archive_write_set_format_ustar.c archive_write_set_format_v7tar.c \
	archive_write_set_format_warc.c archive_write_set_format_xar.c archive_write_set_format_zip.c archive_write_set_passphrase.c \
	archive_write_set_options.c filter_fork_posix.c

CFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-DPLATFORM_CONFIG_H=\"${.CURDIR}/config_ravynos.h\" \
	-DHAVE_BZLIB_H=1 -DHAVE_LIBLZMA=1 -DHAVE_LZMA_H=1 -DHAVE_SYSCONF=1 \
	-I${.CURDIR}/src/libarchive -I${ROOT_SOURCE_DIR}/BSD/lib/libmd \
	-I${ROOT_SOURCE_DIR}/Libraries/zlib -I${ROOT_SOURCE_DIR}/BSD/lib/bzip2/src \
	-I${ROOT_SOURCE_DIR}/Frameworks/CoreText/xz/src/liblzma/api \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h

all: lib${LIB}.a

.include <bsd.lib.mk>
