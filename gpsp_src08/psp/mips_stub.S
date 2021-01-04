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

.global mips_update_gba
.global mips_indirect_branch_arm
.global mips_indirect_branch_thumb
.global mips_indirect_branch_dual
.global execute_load_u8
.global execute_load_u16
.global execute_load_u32
.global execute_load_s8
.global execute_load_s16
.global execute_store_u8
.global execute_store_u16
.global execute_store_u32
.global execute_aligned_load32
.global execute_aligned_store32
.global execute_read_cpsr
.global execute_read_spsr
.global execute_swi
.global execute_spsr_restore
.global execute_store_cpsr
.global execute_store_spsr
.global execute_lsl_flags_reg
.global execute_lsr_flags_reg
.global execute_asr_flags_reg
.global execute_ror_flags_reg
.global execute_arm_translate
.global invalidate_icache_region
.global step_debug_mips
.global reg_check

.global memory_map_read
.global memory_map_write
.global reg

.extern reg
.extern spsr

# MIPS register layout:

# $0 - constant zero
# $1 - temporary
# $2 - temporary / return value
# $3 - ARM r0 (not saved)
# $4 - temporary / function argument 0
# $5 - temporary / function argument 1
# $6 - temporary / function argument 2
# $7 - ARM r1 (not saved)
# $8 - ARM r2 (not saved)
# $9 - ARM r3 (not saved)
# $10 - ARM r4 (not saved)
# $11 - ARM r5 (not saved)
# $12 - ARM r6 (not saved)
# $13 - ARM r7 (not saved)
# $14 - ARM r8 (not saved)
# $15 - ARM r9 (not saved)
# $16 - ARM machine state pointer (saved)
# $17 - cycle counter (saved)
# $18 - ARM r10 (saved)
# $19 - block start address (roughly r15) (saved)
# $20 - ARM negative register (saved)
# $21 - ARM zero register (saved)
# $22 - ARM carry register (saved)
# $23 - ARM overflow register (saved)
# $24 - ARM r11 (not saved)
# $25 - ARM r12 (not saved)
# $26 - kernel temporary 0
# $27 - kernel temporary 1
# $28 - ARM r13 (saved)
# $29 - stack pointer
# $30 - ARM r14 (saved)
# $31 - return address

.equ REG_R0,              (0 * 4)
.equ REG_R1,              (1 * 4)
.equ REG_R2,              (2 * 4)
.equ REG_R3,              (3 * 4)
.equ REG_R4,              (4 * 4)
.equ REG_R5,              (5 * 4)
.equ REG_R6,              (6 * 4)
.equ REG_R7,              (7 * 4)
.equ REG_R8,              (8 * 4)
.equ REG_R9,              (9 * 4)
.equ REG_R10,             (10 * 4)
.equ REG_R11,             (11 * 4)
.equ REG_R12,             (12 * 4)
.equ REG_R13,             (13 * 4)
.equ REG_R14,             (14 * 4)
.equ REG_PC,              (15 * 4)
.equ REG_LR,              (14 * 4)
.equ REG_PC,              (15 * 4)
.equ REG_N_FLAG,          (16 * 4)
.equ REG_Z_FLAG,          (17 * 4)
.equ REG_C_FLAG,          (18 * 4)
.equ REG_V_FLAG,          (19 * 4)
.equ REG_CPSR,            (20 * 4)
.equ CPU_MODE,            (29 * 4)
.equ CPU_HALT_STATE,      (30 * 4)
.equ CHANGED_PC_STATUS,   (31 * 4)
.equ GP_SAVE,             (32 * 4)

.equ SUPERVISOR_LR,       (reg_mode + (3 * (7 * 4)) + (6 * 4))
.equ SUPERVISOR_SPSR,     (spsr + (3 * 4))

.set noat
.set noreorder

# make sure $16 has the register base for these macros

.macro collapse_flag flag_reg, shift
  ins $2, $\flag_reg, \shift, 1    # insert flag into CPSR
.endm

.macro collapse_flags
  lw $2, REG_CPSR($16)            # load CPSR
  andi $2, $2, 0xFF               # isolate lower 8bits
  collapse_flag 20, 31            # store flags
  collapse_flag 21, 30
  collapse_flag 22, 29
  collapse_flag 23, 28
  sw $2, REG_CPSR($16)            # store CPSR
.endm

.macro extract_flag shift, flag_reg
  ext $\flag_reg, $1, \shift, 1   # extract flag from CPSR
.endm

.macro extract_flags_body         # extract flags from $1
  extract_flag 31, 20             # load flags
  extract_flag 30, 21
  extract_flag 29, 22
  extract_flag 28, 23
.endm

.macro extract_flags
  lw $1, REG_CPSR($16)            # load CPSR
  extract_flags_body
.endm

.macro save_registers
  sw $3, REG_R0($16)
  sw $7, REG_R1($16)
  sw $8, REG_R2($16)
  sw $9, REG_R3($16)
  sw $10, REG_R4($16)
  sw $11, REG_R5($16)
  sw $12, REG_R6($16)
  sw $13, REG_R7($16)
  sw $14, REG_R8($16)
  sw $15, REG_R9($16)
  sw $24, REG_R11($16)
  sw $25, REG_R12($16)

  sw $18, REG_R10($16)
  sw $28, REG_R13($16)
  sw $30, REG_R14($16)

  lw $28, GP_SAVE($16)
.endm

.macro restore_registers
  lw $3, REG_R0($16)
  lw $7, REG_R1($16)
  lw $8, REG_R2($16)
  lw $9, REG_R3($16)
  lw $10, REG_R4($16)
  lw $11, REG_R5($16)
  lw $12, REG_R6($16)
  lw $13, REG_R7($16)
  lw $14, REG_R8($16)
  lw $15, REG_R9($16)
  lw $24, REG_R11($16)
  lw $25, REG_R12($16)

  lw $18, REG_R10($16)
  lw $28, REG_R13($16)
  lw $30, REG_R14($16)
.endm

# Process a hardware event. Since an interrupt might be
# raised we have to check if the PC has changed.

# $4: next address
# $16: register base
# $17: cycle counter

.balign 64

mips_update_gba:
  sw $4, REG_PC($16)              # current PC = $4
  addiu $sp, $sp, -4              # make room on the stack
  sw $ra,($sp)                    # save return address
  collapse_flags                  # update cpsr
  save_registers                  # save registers
  jal update_gba                  # process the next event
  sw $0, CHANGED_PC_STATUS($16)

  lw $ra, ($sp)                   # restore return address
  addiu $sp, $sp, 4               # fix stack

  lw $1, CHANGED_PC_STATUS($16)
  bne $1, $0, lookup_pc
  addu $17, $2, $0                # $17 = new cycle count (delay slot)

  restore_registers

  jr $ra                          # if not, go back to caller
  nop

# Perform an indirect branch.

# $4: GBA address to branch to

mips_indirect_branch_arm:
  save_registers
  jal block_lookup_address_arm    # $2 = MIPS address to jump to
  nop
  restore_registers
  jr $2                           # jump to it
  nop

mips_indirect_branch_thumb:
  save_registers
  jal block_lookup_address_thumb  # $2 = MIPS address to jump to
  nop
  restore_registers
  jr $2                           # jump to it
  nop

mips_indirect_branch_dual:
#  sw $6, REG_PC($16)
  save_registers
  jal block_lookup_address_dual   # $2 = MIPS address to jump to
  nop
  restore_registers
  jr $2                           # jump to it
  nop


# $4: address to write to

execute_load_u8:
  srl $1, $4, 28                  # check if the address is out of range
  bne $1, $0, ext_load_u8         # if it is, perform an extended read
  srl $2, $4, 15                  # $1 = page number of address
  sll $2, $2, 2                   # adjust to word index
  addu $2, $2, $16                # $1 = memory_map_read[address >> 15]
  lw $1, -32768($2)
  beq $1, $0, ext_load_u8         # if it's NULL perform an extended read
  andi $2, $4, 0x7FFF             # $2 = low 15bits of address (delay slot)
  addu $1, $1, $2                 # add the memory map offset
  jr $ra                          # return
  lbu $2, ($1)                    # read the value

ext_load_u8:
  addiu $sp, $sp, -4              # make room on the stack for $ra
  sw $ra, ($sp)                   # store return address
  save_registers
  jal read_memory8                # read the value
  nop
  restore_registers
  lw $ra, ($sp)                   # restore return address
  jr $ra                          # return
  addiu $sp, $sp, 4               # fix stack (delay slot)

execute_load_s8:
  srl $1, $4, 28                  # check if the address is out of range
  bne $1, $0, ext_load_s8         # if it is, perform an extended read
  srl $2, $4, 15                  # $1 = page number of address
  sll $2, $2, 2                   # adjust to word index
  addu $2, $2, $16                # $1 = memory_map_read[address >> 15]
  lw $1, -32768($2)
  beq $1, $0, ext_load_s8         # if it's NULL perform an extended read
  andi $2, $4, 0x7FFF             # $2 = low 15bits of address (delay slot)
  addu $1, $1, $2                 # add the memory map offset
  jr $ra                          # return
  lb $2, ($1)                     # read the value

ext_load_s8:
  addiu $sp, $sp, -4              # make room on the stack for $ra
  sw $ra, ($sp)                   # store return address
  save_registers
  jal read_memory8                # read the value
  nop
  restore_registers
  seb $2, $2                      # sign extend the read value
  lw $ra, ($sp)                   # restore return address
  jr $ra                          # return
  addiu $sp, $sp, 4               # fix stack (delay slot)

execute_load_u16:
  srl $1, $4, 28                  # check if the address is out of range
  ins $1, $4, 4, 1                # or unaligned (bottom bit)
  bne $1, $0, ext_load_u16        # if it is, perform an extended read
  srl $2, $4, 15                  # $1 = page number of address
  sll $2, $2, 2                   # adjust to word index
  addu $2, $2, $16                # $1 = memory_map_read[address >> 15]
  lw $1, -32768($2)
  beq $1, $0, ext_load_u16        # if it's NULL perform an extended read
  andi $2, $4, 0x7FFF             # $2 = low 15bits of address (delay slot)
  addu $1, $1, $2                 # add the memory map offset
  jr $ra                          # return
  lhu $2, ($1)                    # read the value

ext_load_u16:
  addiu $sp, $sp, -4              # make room on the stack for $ra
  sw $ra, ($sp)                   # store return address
  save_registers
  jal read_memory16               # read the value
  nop
  restore_registers
  lw $ra, ($sp)                   # restore return address
  jr $ra                          # return
  addiu $sp, $sp, 4               # fix stack (delay slot)

execute_load_s16:
  srl $1, $4, 28                  # check if the address is out of range
  ins $1, $4, 4, 1                # or unaligned (bottom bit)
  bne $1, $0, ext_load_s16        # if it is, perform an extended read
  srl $2, $4, 15                  # $1 = page number of address
  sll $2, $2, 2                   # adjust to word index
  addu $2, $2, $16                # $1 = memory_map_read[address >> 15]
  lw $1, -32768($2)
  beq $1, $0, ext_load_s16        # if it's NULL perform an extended read
  andi $2, $4, 0x7FFF             # $2 = low 15bits of address (delay slot)
  addu $1, $1, $2                 # add the memory map offset
  jr $ra                          # return
  lh $2, ($1)                     # read the value

ext_load_s16:
  addiu $sp, $sp, -4              # make room on the stack for $ra
  sw $ra, ($sp)                   # store return address
  save_registers
  jal read_memory16_signed        # read the value
  nop
  restore_registers
  seh $2, $2                      # sign extend the return value
  lw $ra, ($sp)                   # restore return address
  jr $ra                          # return
  addiu $sp, $sp, 4               # fix stack (delay slot)

execute_load_u32:
  srl $1, $4, 28                  # check if the address is out of range
  ins $1, $4, 4, 2                # or unaligned (bottom two bits)
  bne $1, $0, ext_load_u32        # if it is, perform an extended read
  srl $2, $4, 15                  # $1 = page number of address
  sll $2, $2, 2                   # adjust to word index
  addu $2, $2, $16                # $1 = memory_map_read[address >> 15]
  lw $1, -32768($2)
  beq $1, $0, ext_load_u32        # if it's NULL perform an extended read
  andi $2, $4, 0x7FFF             # $2 = low 15bits of address (delay slot)
  addu $1, $1, $2                 # add the memory map offset
  jr $ra                          # return
  lw $2, ($1)                     # read the value

ext_load_u32:
  addiu $sp, $sp, -4              # make room on the stack for $ra
  sw $ra, ($sp)                   # store return address
  save_registers
  jal read_memory32               # read the value
  nop
  restore_registers
  lw $ra, ($sp)                   # restore return address
  jr $ra                          # return
  addiu $sp, $sp, 4               # fix stack (delay slot)

execute_aligned_load32:
  srl $2, $4, 28                  # check if the address is out of range
  bne $2, $0, ext_aligned_load32  # if it is, perform an extended load
  srl $1, $4, 15                  # $1 = page number of address
  sll $1, $1, 2                   # adjust to word index
  addu $1, $1, $16                # $1 = memory_map_read[address >> 15]
  lw $1, -32768($1)
  beq $1, $0, ext_aligned_load32  # if it's NULL perform an extended read
  andi $2, $4, 0x7FFF             # $2 = low 15bits of address (delay slot)
  addu $1, $1, $2                 # add the memory map offset
  jr $ra                          # return
  lw $2, ($1)                     # read the value

ext_aligned_load32:
  addiu $sp, $sp, -8              # make room on the stack for $ra
  sw $6, 4($sp)
  sw $ra, ($sp)                   # store return address
  save_registers
  jal read_memory32               # read the value
  nop
  restore_registers
  lw $6, 4($sp)
  lw $ra, ($sp)                   # restore return address
  jr $ra                          # return
  addiu $sp, $sp, 8               # fix stack (delay slot)

# $4: address to write to
# $5: value to write
# $6: current PC

execute_store_u8:
  srl $1, $4, 28                  # check if the address is out of range
  bne $1, $0, ext_store_u8        # if it is, perform an extended write
  srl $2, $4, 15                  # $1 = page number of address (delay slot)
  sll $2, $2, 2                   # adjust to word index
  addu $2, $2, $16
  lw $1, 256($2)                  # $1 = memory_map_write[address >> 15]
  beq $1, $0, ext_store_u8        # if it's NULL perform an extended write
  andi $2, $4, 0x7FFF             # $2 = low 15bits of address (delay slot)
  addu $1, $1, $2                 # add the memory map offset
  lb $2, -32768($1)               # load the SMC status
  bne $2, $0, smc_write           # is there code there?
  sb $5, ($1)                     # store the value (delay slot)
  jr $ra                          # return
  nop

ext_store_u8:
  addiu $sp, $sp, -4              # make room on the stack for $ra
  sw $ra, ($sp)
  save_registers
  jal write_memory8               # write the value out
  sw $6, REG_PC($16)              # save the PC (delay slot)

write_epilogue:
  beq $2, $0, no_alert            # 0 means nothing happened
  addiu $4, $2, -2                # see if return value is 2 (delay slot)
  beq $4, $0, smc_dma             # is it an SMC alert? (return value = 2)
  nop
  collapse_flags                  # make sure flags are good for update_gba

alert_loop:
  jal update_gba                  # process the next event
  nop
  lw $1, CPU_HALT_STATE($16)      # check if CPU is sleeping
  bne $1, $0, alert_loop          # see if it hasn't changed
  nop

  addu $17, $2, $0                # $17 = new cycle counter
  lw $4, REG_PC($16)              # $4 = new PC

  j lookup_pc
  addiu $sp, $sp, 4               # fix the stack (delay slot)

# I don't know why this is necessary. But it is. >_>
# Try Golden Sun with this removed.. it's probably indicative
# of a memory leak somewhere else. I don't know.
.balign 64

smc_dma:
  addiu $sp, $sp, 4               # fix the stack
  jal flush_translation_cache_ram # flush translation cache
  nop
  j lookup_pc
  nop

# See above. v_v
.balign 64

no_alert:
  restore_registers

  lw $ra, ($sp)                   # restore return address
  jr $ra                          # we can return
  addiu $sp, $sp, 4               # fix the stack

execute_store_u16:
  ins $4, $0, 0, 1                # zero out bottom bit (alignment)
  srl $1, $4, 28                  # check if the address is out of range
  bne $1, $0, ext_store_u16       # if it is, perform an extended write
  srl $2, $4, 15                  # $1 = page number of address (delay slot)
  sll $2, $2, 2                   # adjust to word index
  addu $2, $2, $16
  lw $1, 256($2)                  # $1 = memory_map_write[address >> 15]
  beq $1, $0, ext_store_u16        # if it's NULL perform an extended write
  andi $2, $4, 0x7FFF             # $2 = low 15bits of address (delay slot)
  addu $1, $1, $2                 # add the memory map offset
  lh $2, -32768($1)               # load the SMC status
  bne $2, $0, smc_write           # is there code there?
  sh $5, ($1)                     # store the value (delay slot)
  jr $ra                          # return
  nop

ext_store_u16:
  addiu $sp, $sp, -4              # make room on the stack for $ra
  sw $ra, ($sp)
  save_registers
  jal write_memory16              # write the value out
  sw $6, REG_PC($16)              # save the PC (delay slot)
  j write_epilogue
  nop

execute_store_u32:
  ins $4, $0, 0, 2                # zero out bottom two bits (alignment)
  srl $1, $4, 28                  # check if the address is out of range
  bne $1, $0, ext_store_u32       # if it is, perform an extended write
  srl $2, $4, 15                  # $1 = page number of address (delay slot)
  sll $2, $2, 2                   # adjust to word index
  addu $2, $2, $16
  lw $1, 256($2)                  # $1 = memory_map_write[address >> 15]
  beq $1, $0, ext_store_u32       # if it's NULL perform an extended write
  andi $2, $4, 0x7FFF             # $2 = low 15bits of address (delay slot)
  addu $1, $1, $2                 # add the memory map offset
  lw $2, -32768($1)               # load the SMC status
  bne $2, $0, smc_write           # is there code there?
  sw $5, ($1)                     # store the value (delay slot)
  jr $ra                          # return
  nop

ext_store_u32:
  addiu $sp, $sp, -4              # make room on the stack for $ra
  sw $ra, ($sp)
  save_registers
  jal write_memory32              # write the value out
  sw $6, REG_PC($16)              # save the PC (delay slot)
  j write_epilogue
  nop

execute_aligned_store32:
  srl $2, $4, 28                  # check if the address is out of range
  bne $2, $0, ext_aligned_store32 # if it is, perform an extended load
  srl $1, $4, 15                  # $1 = page number of address
  sll $1, $1, 2                   # adjust to word index
  addu $1, $1, $16                # $1 = memory_map_write[address >> 15]
  lw $1, 256($1)
  beq $1, $0, ext_aligned_store32 # if it's NULL perform an extended write
  andi $2, $4, 0x7FFF             # $2 = low 15bits of address (delay slot)
  addu $1, $1, $2                 # add the memory map offset
  jr $ra                          # return
  sw $5, ($1)                     # write the value

ext_aligned_store32:
  addiu $sp, $sp, -8              # make room on the stack for $ra, $6
  sw $6, 4($sp)
  sw $ra, ($sp)                   # store return address
  save_registers
  jal write_memory32              # read the value
  nop
  restore_registers
  lw $6, 4($sp)
  lw $ra, ($sp)                   # restore return address
  jr $ra                          # return
  addiu $sp, $sp, 8               # fix stack (delay slot)

smc_write:
  save_registers
  jal flush_translation_cache_ram # flush translation cache
  sw $6, REG_PC($16)              # save PC (delay slot)

lookup_pc:
  lw $2, REG_CPSR($16)            # $2 = cpsr
  andi $2, $2, 0x20               # isolate mode bit
  beq $2, $0, lookup_pc_arm       # if T bit is zero use arm handler
  nop

lookup_pc_thumb:
  jal block_lookup_address_thumb  # get Thumb address
  lw $4, REG_PC($16)              # load PC as arg 0 (delay slot)
  restore_registers
  jr $2                           # jump to result
  nop

lookup_pc_arm:
  jal block_lookup_address_arm    # get ARM address
  lw $4, REG_PC($16)              # load PC as arg 0 (delay slot)
  restore_registers
  jr $2                           # jump to result
  nop

# Return the current cpsr

execute_read_cpsr:
  collapse_flags                  # fold flags into cpsr, put cpsr into $2
  jr $ra                          # return
  nop

# Return the current spsr

execute_read_spsr:
  lw $1, CPU_MODE($16)            # $1 = cpu_mode
  lui $2, %hi(spsr)
  sll $1, $1, 2                   # adjust to word offset size
  addu $2, $2, $1
  jr $ra                          # return
  lw $2, %lo(spsr)($2)            # $2 = spsr[cpu_mode] (delay slot)

# Switch into SWI, has to collapse flags
# $4: Current pc

execute_swi:
  add $sp, $sp, -4                # push $ra
  sw $ra, ($sp)
  lui $1, %hi(SUPERVISOR_LR)
  sw $4, %lo(SUPERVISOR_LR)($1)   # store next PC in the supervisor's LR
  collapse_flags                  # get cpsr in $2
  lui $5, %hi(SUPERVISOR_SPSR)
  sw $2, %lo(SUPERVISOR_SPSR)($5) # save cpsr in SUPERVISOR_CPSR
  ins $2, $0, 0, 6                # zero out bottom 6 bits of CPSR
  ori $2, 0x13                    # set mode to supervisor
  sw $2, REG_CPSR($16)            # write back CPSR
  save_registers
  jal set_cpu_mode                # set the CPU mode to supervisor
  li $4, 3                        # 3 is supervisor mode (delay slot)
  restore_registers
  lw $ra, ($sp)                   # pop $ra
  jr $ra                          # return
  add $sp, $sp, 4                 # fix stack (delay slot)

# $4: pc to restore to
# returns in $4

execute_spsr_restore:
  lw $1, CPU_MODE($16)            # $1 = cpu_mode

  lui $2, %hi(spsr)
  sll $1, $1, 2                   # adjust to word offset size
  addu $2, $2, $1
  lw $1, %lo(spsr)($2)            # $1 = spsr[cpu_mode]
  sw $1, REG_CPSR($16)            # cpsr = spsr[cpu_mode]
  extract_flags_body              # extract flags from $1
  addiu $sp, $sp, -4
  sw $ra, ($sp)
  save_registers
  jal execute_spsr_restore_body   # do the dirty work in this C function
  nop
  restore_registers
  addu $4, $2, $0                 # move return value to $4
  lw $ra, ($sp)
  jr $ra
  addiu $sp, $sp, 4

# $4: new cpsr
# $5: store mask
# $6: current PC

execute_store_cpsr:
  and $1, $4, $5                  # $1 = new_cpsr & store_mask
  lw $2, REG_CPSR($16)            # $2 = current cpsr
  nor $4, $5, $0                  # $4 = ~store_mask
  and $2, $2, $4                  # $2 = (cpsr & (~store_mask))
  or $1, $1, $2                   # $1 = new cpsr combined with old
  extract_flags_body              # extract flags from $1
  addiu $sp, $sp, -4
  sw $ra, ($sp)
  save_registers
  jal execute_store_cpsr_body     # do the dirty work in this C function
  addu $4, $1, $0                 # load the new CPSR (delay slot)

  bne $2, $0, changed_pc_cpsr     # this could have changed the pc
  nop

  restore_registers

  lw $ra, ($sp)
  jr $ra
  addiu $sp, $sp, 4

changed_pc_cpsr:
  jal block_lookup_address_arm    # GBA address is in $4
  addu $4, $2, $0                 # load new address in $4 (delay slot)
  restore_registers               # restore registers
  jr $2                           # jump to the new address
  addiu $sp, $sp, 4               # get rid of the old ra (delay slot)


# $4: new spsr
# $5: store mask

execute_store_spsr:
  lw $1, CPU_MODE($16)            # $1 = cpu_mode
  lui $2, %hi(spsr)
  sll $1, $1, 2                   # adjust to word offset size
  addu $1, $2, $1
  lw $2, %lo(spsr)($1)            # $2 = spsr[cpu_mode]
  and $4, $4, $5                  # $4 = new_spsr & store_mask
  nor $5, $5, $0                  # $5 = ~store_mask
  and $2, $2, $5                  # $2 = (spsr & (~store_mask))
  or $4, $4, $2                   # $4 = new spsr combined with old
  jr $ra                          # return
  sw $4, %lo(spsr)($1)            # spsr[cpu_mode] = $4 (delay slot)

# $4: value
# $5: shift

execute_lsl_flags_reg:
  beq $5, $0, lsl_zero_shift      # is the shift zero?
  sltiu $1, $5, 32                # $1 = (shift < 32) (delay)
  bne $1, $0, lsl_low_shift       # is the shift < 32?
  andi $5, $5, 0x1F               # only interested in the low 5 bits
  sll $4, $4, 16                  # value <<= 16
  addiu $5, $5, -16               # shift -= 16

lsl_low_shift:
  li $1, 32                       # $1 = 32
  subu $1, $1, $5                 # $1 = (32 - shift)
  srlv $1, $4, $1                 # $1 = (value >> (32 - shift))
  andi $22, $1, 1                 # c flag = $1 & 1

lsl_zero_shift:
  jr $ra                          # return
  sllv $4, $4, $5                 # return (value << shift) (delay)


execute_lsr_flags_reg:
  beq $5, $0, lsr_zero_shift      # is the shift zero?
  sltiu $1, $5, 32                # $1 = (shift < 32) (delay)
  bne $1, $0, lsr_low_shift       # is the shift < 32?
  andi $5, $5, 0x1F               # only interested in the low 5 bits
  srl $4, $4, 16                  # value >>= 16
  addiu $5, $5, -16               # shift -= 16

lsr_low_shift:
  addiu $1, $5, -1                # $1 = (shift - 1)
  srlv $1, $4, $1                 # $1 = (value >> (shift - 1))
  andi $22, $1, 1                 # c flag = $1 & 1

lsr_zero_shift:
  jr $ra                          # return
  srlv $4, $4, $5                 # return (value >> shift) delay


execute_asr_flags_reg:
  beq $5, $0, asr_zero_shift      # is the shift zero?
  sltiu $1, $5, 32                # $1 = (shift < 32) (delay)
  bne $1, $0, asr_low_shift       # is the shift < 32?
  andi $5, $5, 0x1F               # only interested in the low 5 bits
  sra $4, $4, 16                  # value >>= 16
  addiu $5, $5, -16               # shift -= 16

asr_low_shift:
  addiu $1, $5, -1                # $1 = (shift - 1)
  srlv $1, $4, $1                 # $1 = (value >> (shift - 1))
  andi $22, $1, 1                 # c flag = $1 & 1

asr_zero_shift:
  jr $ra                          # return
  srav $4, $4, $5                 # return (value >> shift) delay

execute_ror_flags_reg:
  beq $5, $0, ror_zero_shift      # is the shift zero?
  addiu $1, $5, -1                # $1 = (shift - 1) (delay)

  srlv $1, $4, $1                 # $1 = (value >> (shift - 1))
  andi $22, $1, 1                 # c flag = $1 & 1

ror_zero_shift:
  jr $ra                          # return
  rotrv $4, $4, $5                # return (value ror shift) delay

# $4: cycle counter argument

execute_arm_translate:
  addu $17, $4, $0                # load cycle counter register
  lui $16, %hi(reg)               # load base register
  addiu $16, %lo(reg)
  extract_flags                   # load flag variables
  lw $4, REG_PC($16)              # load PC into $4
  jal block_lookup_address_arm    # lookup jump address for it
  nop
  jr $2                           # jump to it
  nop

# sceKernelInvalidateIcacheRange gives me problems, trying this instead
# Invalidates an n byte region starting at the start address
# $4: start location
# $5: length

invalidate_icache_region:
  ins $4, $0, 0, 6                # align to 64 bytes
  addiu $2, $5, 63                # align up to 64 bytes
  srl $2, $2, 6                   # divide by 64
  beq $2, $0, done                # exit early on 0
  nop

iir_loop:
  cache 8, 0($4)                  # invalidate icache line
  addiu $2, $2, -1                # next loop iteration
  bne $2, $0, iir_loop            # loop
  addiu $4, $4, 64                # go to next cache line (delay slot)

done:
  jr $ra                          # return
  nop

step_debug_mips:
  addiu $sp, $sp, -4
  sw $ra, ($sp)
  collapse_flags
  save_registers
  jal step_debug
  addiu $5, $17, 0
  restore_registers
  lw $ra, ($sp)
  jr $ra
  addiu $sp, $sp, 4

memory_map_read:
  .space 0x8000

reg:
  .space 0x100

memory_map_write:
  .space 0x8000




