(* this is technically a program header? *)
type section = { addr : int32; offset : int32; size : int32; flags : int32 }

type t = {
  header_size : int;
  prog_header_size : int;
  text : section;
  rdata : section;
  bss : section;
}

let elf_header b c =
  let open Bytebuffer in
  (* ELFMAG: \177ELF *)
  set_u8 b 0x7f;
  set_string b "ELF";
  (* EI_CLASS: 32-bit *)
  set_u8 b 1;
  (* EI_DATA: Little-endian *)
  set_u8 b 1;
  (* EI_VERSION: Current *)
  set_u8 b 1;
  (* EI_OSABI: No extensions *)
  set_u8 b 0;
  (* EI_ABIVERSION *)
  set_u8 b 0;
  (* EI_PAD ... (7 bytes) *)
  pad_u8 b 7 0;
  (* e_type: ET_EXEC *)
  set_u16 b 2;
  (* e_machine: EM_386 *)
  set_u16 b 3;
  (* e_version: EV_CURRENT *)
  set_u32 b 1l;
  (* e_entry *)
  set_u32 b c.text.addr;
  (* e_phoff *)
  set_u32 b (Int32.of_int c.header_size);
  (* e_shoff *)
  set_u32 b 0l;
  (* e_flags *)
  set_u32 b 0l;
  (* e_ehsize *)
  set_u16 b c.header_size;
  (* e_phentsize *)
  set_u16 b c.prog_header_size;
  (* e_phnum *)
  set_u16 b 3;
  (* e_shentsize *)
  set_u16 b 40;
  (* e_shnum *)
  set_u16 b 0;
  (* e_shstrndx *)
  set_u16 b 0

let align_to value align =
  let rem = value mod align in
  if rem = 0 then value else value + (align - rem)

let prog_headers b c =
  let page_size = 4096l in
  let make_header s =
    let open Bytebuffer in
    (* p_type: PT_LOAD *)
    set_u32 b 1l;
    (* p_offset *)
    set_u32 b s.offset;
    (* p_vaddr *)
    set_u32 b s.addr;
    (* p_paddr *)
    set_u32 b 0l;
    (* p_filesz: if the sectio writable then 0. ugly hack for bss *)
    set_u32 b (if Int32.(logand s.flags 0x2l) <> 0l then 0l else s.size);
    (* p_memsz *)
    set_u32 b s.size;
    (* p_flags: PF_R | PF_X *)
    set_u32 b s.flags;
    (* p_align *)
    set_u32 b page_size
  in
  (* let text_offset =
       align_to (c.header_size + (3 * c.prog_header_size)) (Int32.to_int page_size)
     in
     let rdata_offset =
       align_to (text_offset + c.text.size) (Int32.to_int page_size)
     in *)
  make_header c.text;
  make_header c.rdata;
  make_header c.bss
(* make_header (Int32.of_int text_offset) c.text_addr (Int32.of_int c.text_size)
     (Int32.logor 0x4l 0x1l);
   make_header
     (Int32.of_int rdata_offset)
     c.rdata_addr
     (Int32.of_int c.rdata_size)
     0x4l *)
