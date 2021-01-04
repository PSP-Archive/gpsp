# gameplaySP
#
# Copyright (C) 2006 Exophase <exophase@gmail.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

.align 4

.global _x86_update_gba
.global _x86_indirect_branch_arm
.global _x86_indirect_branch_thumb
.global _x86_indirect_branch_dual
.global _execute_store_u8
.global _execute_store_u16
.global _execute_store_u32
.global _execute_arm_translate
.global _step_debug_x86

.global _memory_map_read
.global _memory_map_write
.global _reg

.global _iwram
.global _ewram
.global _vram
.global _bios_rom
.global _io_registers

.extern _spsr

.equ REG_SP,            (13 * 4)
.equ REG_LR,            (14 * 4)
.equ REG_PC,            (15 * 4)
.equ REG_N_FLAG,        (16 * 4)
.equ REG_Z_FLAG,        (17 * 4)
.equ REG_C_FLAG,        (18 * 4)
.equ REG_V_FLAG,        (19 * 4)
.equ REG_CPSR,          (20 * 4)
.equ CPU_MODE,          (29 * 4)
.equ CPU_HALT_STATE,    (30 * 4)
.equ CHANGED_PC_STATUS, (31 * 4)

# destroys ecx and edx

.macro collapse_flag offset, shift
  mov _reg + \offset, %ecx
  shl $\shift, %ecx
  or %ecx, %edx
.endm

.macro collapse_flags
  xor %edx, %edx
  collapse_flag REG_N_FLAG, 31
  collapse_flag REG_Z_FLAG, 30
  collapse_flag REG_C_FLAG, 29
  collapse_flag REG_V_FLAG, 28
  mov REG_CPSR(%ebx), %ecx
  and $0xFF, %ecx
  or %ecx, %edx
  mov %edx, REG_CPSR(%ebx)
.endm

.macro extract_flag shift, offset
  mov REG_CPSR(%ebx), %edx
  shr $\shift, %edx
  and $0x01, %edx
  mov %edx, _reg + \offset
.endm

.macro extract_flags
  extract_flag 31, REG_N_FLAG
  extract_flag 30, REG_Z_FLAG
  extract_flag 29, REG_C_FLAG
  extract_flag 28, REG_V_FLAG
.endm

# Process a hardware event. Since an interrupt might be
# raised we have to check if the PC has changed.

# eax: current address

_x86_update_gba:
  mov %eax, REG_PC(%ebx)      # current PC = eax
  collapse_flags              # update cpsr, trashes ecx and edx
  call _update_gba            # process the next event

  mov %eax, %edi              # edi = new cycle count
  # did the PC change?
  cmpl $1, CHANGED_PC_STATUS(%ebx)
  je lookup_pc
  ret                         # if not, go back to caller

# Perform this on an indirect branch that will definitely go to
# ARM code, IE anything that changes the PC in ARM mode except
# for BX and data processing to PC with the S bit set.

# eax: GBA address to branch to
# edi: Cycle counter

_x86_indirect_branch_arm:
  call _block_lookup_address_arm
  jmp *%eax

# For indirect branches that'll definitely go to Thumb. In
# Thumb mode any indirect branches except for BX.

_x86_indirect_branch_thumb:
  call _block_lookup_address_thumb
  jmp *%eax

# For indirect branches that can go to either Thumb or ARM,
# mainly BX (also data processing to PC with S bit set, be
# sure to adjust the target with a 1 in the lowest bit for this)

_x86_indirect_branch_dual:
  call _block_lookup_address_dual
  jmp *%eax

# eax: address to write to
# edx: value to write
# ecx: current pc

.macro execute_store_ext_builder ext_function
  call \ext_function
  jmp write_epilogue
.endm

_execute_store_u8:
  mov %ecx, REG_PC(%ebx)      # write out the PC
  mov %eax, %ecx              # ecx = address
  test $0xF0000000, %ecx      # check address range
  jnz ext_store_u8            # if above perform an extended write
  shr $15, %ecx               # ecx = page number of address
  # load the corresponding memory map offset
  mov _memory_map_write(, %ecx, 4), %ecx
  test %ecx, %ecx             # see if it's NULL
  jz ext_store_u8             # if so perform an extended write
  and $0x7FFF, %eax           # isolate the lower 15bits of the address
  mov %dl, (%eax, %ecx)       # store the value
  # check for self-modifying code
  testb $0xFF, -32768(%eax, %ecx)
  jne smc_write
  ret                         # return

ext_store_u8:
  execute_store_ext_builder _write_memory8

_execute_store_u16:
  mov %ecx, REG_PC(%ebx)      # write out the PC
  and $~0x01, %eax            # fix alignment
  mov %eax, %ecx              # ecx = address
  test $0xF0000000, %ecx      # check address range
  jnz ext_store_u16           # if above perform an extended write
  shr $15, %ecx               # ecx = page number of address
  # load the corresponding memory map offset
  mov _memory_map_write(, %ecx, 4), %ecx
  test %ecx, %ecx             # see if it's NULL
  jz ext_store_u16            # if so perform an extended write
  and $0x7FFF, %eax           # isolate the lower 15bits of the address
  mov %dx, (%eax, %ecx)       # store the value
  # check for self-modifying code
  testw $0xFFFF, -32768(%eax, %ecx)
  jne smc_write
  ret                         # return

ext_store_u16:
  execute_store_ext_builder _write_memory16

_execute_store_u32:
  mov %ecx, REG_PC(%ebx)      # write out the PC
  and $~0x03, %eax            # fix alignment
  mov %eax, %ecx              # ecx = address
  test $0xF0000000, %ecx      # check address range
  jnz ext_store_u32           # if above perform an extended write
  shr $15, %ecx               # ecx = page number of address
  # load the corresponding memory map offset
  mov _memory_map_write(, %ecx, 4), %ecx
  test %ecx, %ecx             # see if it's NULL
  jz ext_store_u32            # if so perform an extended write
  and $0x7FFF, %eax           # isolate the lower 15bits of the address
  mov %edx, (%eax, %ecx)      # store the value
  # check for self-modifying code
  testl $0xFFFFFFFF, -32768(%eax, %ecx)
  jne smc_write
  ret                         # return it

ext_store_u32:
  execute_store_ext_builder _write_memory32

write_epilogue:
  cmp $0, %eax                # 0 return means nothing happened
  jz no_alert                 # if so we can leave

  collapse_flags              # make sure flags are good for function call
  cmp $2, %eax                # see if it was an SMC trigger
  je smc_write

alert_loop:
  call _update_gba            # process the next event
  # see if the halt status has changed
  mov CPU_HALT_STATE(%ebx), %edx

  cmp $0, %edx                # 0 means it has
  jnz alert_loop              # if not go again

  mov %eax, %edi              # edi = new cycle count
  jmp lookup_pc               # pc has definitely changed

no_alert:
  ret

#  execute_store_epilogue_builder smc_thumb

str:
  .asciz "looking up pc\n"

smc_write:
  call _flush_translation_cache_ram

lookup_pc:
  add $4, %esp
  movl $0, CHANGED_PC_STATUS(%ebx)
  mov REG_PC(%ebx), %eax
  testl $0x20, REG_CPSR(%ebx)
  jz lookup_pc_arm

lookup_pc_thumb:
  call _block_lookup_address_thumb
  jmp *%eax

lookup_pc_arm:
  call _block_lookup_address_arm
  jmp *%eax

# eax: cycle counter

_execute_arm_translate:
  movl $_reg, %ebx            # load base register
  extract_flags               # load flag variables
  movl %eax, %edi             # load edi cycle counter

  movl REG_PC(%ebx), %eax     # load PC
  call _block_lookup_address_arm
  jmp *%eax                   # jump to it

_step_debug_x86:
  collapse_flags
  mov %edi, %edx
  jmp _step_debug

.comm _memory_map_read 0x8000
.comm _memory_map_write 0x8000
.comm _reg 0x100


