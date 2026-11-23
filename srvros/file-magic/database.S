/* The native compiler and SrvrOS both use the little-endian 5.48 format. */
.section .data.file_magic_db,"aw",@progbits
.balign 8
.global file_magic_db_start
.global file_magic_db_end
file_magic_db_start:
.incbin "build/ports/file-magic/bounded.magic.mgc"
file_magic_db_end:
.section .note.GNU-stack,"",@progbits
