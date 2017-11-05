! 
!   os_solaris_ata.s
! 
!   Home page of code is: http://www.smartmontools.org
! 
!   Copyright (C) 2003-8 SAWADA Keiji <smartmontools-support@lists.sourceforge.net>
! 
!   This program is free software; you can redistribute it and/or modify
!   it under the terms of the GNU General Public License as published by
!   the Free Software Foundation; either version 2 of the License, or
!   (at your option) any later version.
! 
!   This program is distributed in the hope that it will be useful, but
!   WITHOUT ANY WARRANTY; without even the implied warranty of
!   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
!   General Public License for more details.
! 
!   You should have received a copy of the GNU General Public License
!   along with this program; if not, write to the Free Software Foundation,
!   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
! 
! 
!        --------------------------------------------------------
!        direct access routines to ATA device under Solaris/SPARC
!        --------------------------------------------------------
! 
! Information
! -----------
! 
! In Solaris, programmer can pass SCSI command to target device directly
! by using USCSI ioctl or using "scg" generic SCSI driver.  But, such
! method does not exist for ATA devices.
! 
! However, I can access Solaris kernel source because I am subscriber of
! Source Foundation Program of Solaris.  So, I can find method of
! accessing ATA device directly.  The method is to pack command in
! undocumented structure and issue ioctl that appears only in kernel
! source.  Yes, that is the same way in using USCSI interface.
! 
! But, I met difficulty in disclosing this technique.  I have signed NDA
! with Sun that inhibits me not to violate their intellectual property.
! 
! Fortunately, Sun allows licensees to publish "Interfaces" if:
! 
! (1) he/she treats Solaris code as confidential
! 
! (2) and he/she doesn't incorporate Sun's code into his/her code
! 
! (3) and disclose enough information to use "Interface" to everyone.
! 
! So, I publish that technique in assembly code or object code because:
! 
! (1) I believe Sun's intellectural property is not invaded because I
!     didn't reveal any struct member and ioctl to non-licensee.
! 
! (2) no piece of kernel source is included in this code.
! 
! (3) And finally, I publish enough information below in order to use
!     this code.
! 
! For last reason, please don't remove "Calling Interface" section from
! distribution.
! 
! 
! Calling Interface
! -----------------
! 
! Name of function/macro presents corresponding S.M.A.R.T. command.
! 
! Parameters are described below.
! 
! int fd
! 
!     File descriptor of ATA device.  Device would be
!     /dev/rdsk/cXtXdXsX.
! 
!     Device should be raw device serviced by "dada" driver.  ATAPI
!     CD-ROM/R/RW, DVD-ROM, and so on are not allowed because they are
!     serviced by "sd" driver.  On x86 Solaris, "cmdk" driver services
!     them, this routines doesn't work.
! 
! int s
!     Select sector for service.  For example, this indicates log sector
!     number for smart_read_log() function.  Probably you need to read
!     ATA specification for this parameter.
! 
! void *data
!     Data going to be read/written.  It don't have to be word aligned,
!     But data shall points valid user memory space.
! 
! This is very tiny routines, but if you feel this insufficient, please
! let me know.
! 
! 					ksw / SAWADA Keiji
! 					<card_captor@users.sourceforge.net>
	.file	"solaris-ata-in.c"
	.section	".rodata"
	.align 8
.LLC0:
	.asciz	"$Id$"
	.global os_solaris_ata_s_cvsid
	.section	".data"
	.align 4
	.type	os_solaris_ata_s_cvsid, #object
	.size	os_solaris_ata_s_cvsid, 4
os_solaris_ata_s_cvsid:
	.long	.LLC0
	.section	".text"
	.align 4
	.type	ata_cmd, #function
	.proc	04
ata_cmd:
	!#PROLOGUE# 0
	save	%sp, -184, %sp
	!#PROLOGUE# 1
	st	%i0, [%fp+68]
	st	%i1, [%fp+72]
	st	%i2, [%fp+76]
	st	%i3, [%fp+80]
	st	%i4, [%fp+84]
	st	%i5, [%fp+88]
	ld	[%fp+92], %g1
	st	%g1, [%fp-76]
	ld	[%fp-76], %g1
	and	%g1, 3, %g1
	cmp	%g1, 0
	be	.LL2
	nop
	mov	-2, %g1
	st	%g1, [%fp-80]
	b	.LL1
	 nop
.LL2:
	add	%fp, -56, %g1
	mov	%g1, %o0
	mov	0, %o1
	mov	36, %o2
	call	memset, 0
	 nop
	add	%fp, -72, %g1
	mov	%g1, %o0
	mov	0, %o1
	mov	16, %o2
	call	memset, 0
	 nop
	ld	[%fp+72], %g1
	stb	%g1, [%fp-72]
	mov	1, %g1
	stb	%g1, [%fp-71]
	mov	1, %g1
	stb	%g1, [%fp-70]
	ld	[%fp+76], %g1
	stb	%g1, [%fp-69]
	ld	[%fp+84], %g1
	sll	%g1, 9, %g1
	st	%g1, [%fp-68]
	ld	[%fp+80], %g1
	st	%g1, [%fp-60]
	mov	10, %g1
	sth	%g1, [%fp-52]
	ld	[%fp+88], %g1
	cmp	%g1, 0
	be	.LL3
	nop
	mov	14, %g1
	st	%g1, [%fp-84]
	b	.LL4
	 nop
.LL3:
	mov	6, %g1
	st	%g1, [%fp-84]
.LL4:
	ld	[%fp-84], %g1
	st	%g1, [%fp-48]
	ld	[%fp+88], %g1
	sll	%g1, 9, %g1
	st	%g1, [%fp-44]
	ld	[%fp+88], %g1
	sll	%g1, 9, %g1
	st	%g1, [%fp-40]
	ld	[%fp+88], %g1
	cmp	%g1, 0
	be	.LL5
	nop
	ld	[%fp+92], %g1
	st	%g1, [%fp-88]
	b	.LL6
	 nop
.LL5:
	st	%g0, [%fp-88]
.LL6:
	ld	[%fp-88], %g1
	st	%g1, [%fp-36]
	add	%fp, -72, %g1
	st	%g1, [%fp-32]
	add	%fp, -56, %g1
	ld	[%fp+68], %o0
	mov	1481, %o1
	mov	%g1, %o2
	call	ioctl, 0
	 nop
	mov	%o0, %g1
	st	%g1, [%fp-80]
.LL1:
	ld	[%fp-80], %i0
	ret
	restore
	.size	ata_cmd, .-ata_cmd
	.align 4
	.global ata_identify
	.type	ata_identify, #function
	.proc	04
ata_identify:
	!#PROLOGUE# 0
	save	%sp, -648, %sp
	!#PROLOGUE# 1
	st	%i0, [%fp+68]
	st	%i1, [%fp+72]
	add	%fp, -536, %g1
	st	%g1, [%sp+92]
	ld	[%fp+68], %o0
	mov	236, %o1
	mov	0, %o2
	mov	0, %o3
	mov	1, %o4
	mov	1, %o5
	call	ata_cmd, 0
	 nop
	mov	%o0, %g1
	st	%g1, [%fp-20]
	add	%fp, -536, %g1
	ld	[%fp+72], %o0
	mov	%g1, %o1
	mov	512, %o2
	call	memcpy, 0
	 nop
	ld	[%fp-20], %g1
	cmp	%g1, 0
	be	.LL8
	nop
	mov	-1, %g1
	st	%g1, [%fp-540]
	b	.LL9
	 nop
.LL8:
	st	%g0, [%fp-540]
.LL9:
	ld	[%fp-540], %g1
	mov	%g1, %i0
	ret
	restore
	.size	ata_identify, .-ata_identify
	.align 4
	.global ata_pidentify
	.type	ata_pidentify, #function
	.proc	04
ata_pidentify:
	!#PROLOGUE# 0
	save	%sp, -648, %sp
	!#PROLOGUE# 1
	st	%i0, [%fp+68]
	st	%i1, [%fp+72]
	add	%fp, -536, %g1
	st	%g1, [%sp+92]
	ld	[%fp+68], %o0
	mov	161, %o1
	mov	0, %o2
	mov	0, %o3
	mov	1, %o4
	mov	1, %o5
	call	ata_cmd, 0
	 nop
	mov	%o0, %g1
	st	%g1, [%fp-20]
	add	%fp, -536, %g1
	ld	[%fp+72], %o0
	mov	%g1, %o1
	mov	512, %o2
	call	memcpy, 0
	 nop
	ld	[%fp-20], %g1
	cmp	%g1, 0
	be	.LL11
	nop
	mov	-1, %g1
	st	%g1, [%fp-540]
	b	.LL12
	 nop
.LL11:
	st	%g0, [%fp-540]
.LL12:
	ld	[%fp-540], %g1
	mov	%g1, %i0
	ret
	restore
	.size	ata_pidentify, .-ata_pidentify
	.align 4
	.global smart_read_data
	.type	smart_read_data, #function
	.proc	04
smart_read_data:
	!#PROLOGUE# 0
	save	%sp, -648, %sp
	!#PROLOGUE# 1
	st	%i0, [%fp+68]
	st	%i1, [%fp+72]
	add	%fp, -536, %g1
	st	%g1, [%sp+92]
	ld	[%fp+68], %o0
	mov	176, %o1
	mov	208, %o2
	sethi	%hi(12733440), %g1
	or	%g1, 768, %o3
	mov	0, %o4
	mov	1, %o5
	call	ata_cmd, 0
	 nop
	mov	%o0, %g1
	st	%g1, [%fp-20]
	add	%fp, -536, %g1
	ld	[%fp+72], %o0
	mov	%g1, %o1
	mov	512, %o2
	call	memcpy, 0
	 nop
	ld	[%fp-20], %g1
	cmp	%g1, 0
	be	.LL14
	nop
	mov	-1, %g1
	st	%g1, [%fp-540]
	b	.LL15
	 nop
.LL14:
	st	%g0, [%fp-540]
.LL15:
	ld	[%fp-540], %g1
	mov	%g1, %i0
	ret
	restore
	.size	smart_read_data, .-smart_read_data
	.align 4
	.global smart_read_thresholds
	.type	smart_read_thresholds, #function
	.proc	04
smart_read_thresholds:
	!#PROLOGUE# 0
	save	%sp, -648, %sp
	!#PROLOGUE# 1
	st	%i0, [%fp+68]
	st	%i1, [%fp+72]
	add	%fp, -536, %g1
	st	%g1, [%sp+92]
	ld	[%fp+68], %o0
	mov	176, %o1
	mov	209, %o2
	sethi	%hi(12733440), %g1
	or	%g1, 769, %o3
	mov	1, %o4
	mov	1, %o5
	call	ata_cmd, 0
	 nop
	mov	%o0, %g1
	st	%g1, [%fp-20]
	add	%fp, -536, %g1
	ld	[%fp+72], %o0
	mov	%g1, %o1
	mov	512, %o2
	call	memcpy, 0
	 nop
	ld	[%fp-20], %g1
	cmp	%g1, 0
	be	.LL17
	nop
	mov	-1, %g1
	st	%g1, [%fp-540]
	b	.LL18
	 nop
.LL17:
	st	%g0, [%fp-540]
.LL18:
	ld	[%fp-540], %g1
	mov	%g1, %i0
	ret
	restore
	.size	smart_read_thresholds, .-smart_read_thresholds
	.align 4
	.global smart_auto_save
	.type	smart_auto_save, #function
	.proc	04
smart_auto_save:
	!#PROLOGUE# 0
	save	%sp, -128, %sp
	!#PROLOGUE# 1
	st	%i0, [%fp+68]
	st	%i1, [%fp+72]
	st	%g0, [%sp+92]
	ld	[%fp+68], %o0
	mov	176, %o1
	mov	210, %o2
	sethi	%hi(12733440), %g1
	or	%g1, 768, %o3
	ld	[%fp+72], %o4
	mov	0, %o5
	call	ata_cmd, 0
	 nop
	mov	%o0, %g1
	st	%g1, [%fp-20]
	ld	[%fp-20], %g1
	cmp	%g1, 0
	be	.LL20
	nop
	mov	-1, %g1
	st	%g1, [%fp-24]
	b	.LL21
	 nop
.LL20:
	st	%g0, [%fp-24]
.LL21:
	ld	[%fp-24], %g1
	mov	%g1, %i0
	ret
	restore
	.size	smart_auto_save, .-smart_auto_save
	.align 4
	.global smart_immediate_offline
	.type	smart_immediate_offline, #function
	.proc	04
smart_immediate_offline:
	!#PROLOGUE# 0
	save	%sp, -128, %sp
	!#PROLOGUE# 1
	st	%i0, [%fp+68]
	st	%i1, [%fp+72]
	ld	[%fp+72], %g1
	and	%g1, 255, %o5
	sethi	%hi(12733440), %g1
	or	%g1, 768, %g1
	or	%o5, %g1, %g1
	st	%g0, [%sp+92]
	ld	[%fp+68], %o0
	mov	176, %o1
	mov	212, %o2
	mov	%g1, %o3
	mov	0, %o4
	mov	0, %o5
	call	ata_cmd, 0
	 nop
	mov	%o0, %g1
	st	%g1, [%fp-20]
	ld	[%fp-20], %g1
	cmp	%g1, 0
	be	.LL23
	nop
	mov	-1, %g1
	st	%g1, [%fp-24]
	b	.LL24
	 nop
.LL23:
	st	%g0, [%fp-24]
.LL24:
	ld	[%fp-24], %g1
	mov	%g1, %i0
	ret
	restore
	.size	smart_immediate_offline, .-smart_immediate_offline
	.align 4
	.global smart_read_log
	.type	smart_read_log, #function
	.proc	04
smart_read_log:
	!#PROLOGUE# 0
	save	%sp, -128, %sp
	!#PROLOGUE# 1
	st	%i0, [%fp+68]
	st	%i1, [%fp+72]
	st	%i2, [%fp+76]
	st	%i3, [%fp+80]
	ld	[%fp+72], %g1
	and	%g1, 255, %o5
	sethi	%hi(12733440), %g1
	or	%g1, 768, %g1
	or	%o5, %g1, %o5
	ld	[%fp+80], %g1
	st	%g1, [%sp+92]
	ld	[%fp+68], %o0
	mov	176, %o1
	mov	213, %o2
	mov	%o5, %o3
	ld	[%fp+76], %o4
	ld	[%fp+76], %o5
	call	ata_cmd, 0
	 nop
	mov	%o0, %g1
	st	%g1, [%fp-20]
	ld	[%fp-20], %g1
	cmp	%g1, 0
	be	.LL26
	nop
	mov	-1, %g1
	st	%g1, [%fp-24]
	b	.LL27
	 nop
.LL26:
	st	%g0, [%fp-24]
.LL27:
	ld	[%fp-24], %g1
	mov	%g1, %i0
	ret
	restore
	.size	smart_read_log, .-smart_read_log
	.align 4
	.global smart_enable
	.type	smart_enable, #function
	.proc	04
smart_enable:
	!#PROLOGUE# 0
	save	%sp, -128, %sp
	!#PROLOGUE# 1
	st	%i0, [%fp+68]
	st	%g0, [%sp+92]
	ld	[%fp+68], %o0
	mov	176, %o1
	mov	216, %o2
	sethi	%hi(12733440), %g1
	or	%g1, 768, %o3
	mov	0, %o4
	mov	0, %o5
	call	ata_cmd, 0
	 nop
	mov	%o0, %g1
	st	%g1, [%fp-20]
	ld	[%fp-20], %g1
	cmp	%g1, 0
	be	.LL29
	nop
	mov	-1, %g1
	st	%g1, [%fp-24]
	b	.LL30
	 nop
.LL29:
	st	%g0, [%fp-24]
.LL30:
	ld	[%fp-24], %g1
	mov	%g1, %i0
	ret
	restore
	.size	smart_enable, .-smart_enable
	.align 4
	.global smart_disable
	.type	smart_disable, #function
	.proc	04
smart_disable:
	!#PROLOGUE# 0
	save	%sp, -128, %sp
	!#PROLOGUE# 1
	st	%i0, [%fp+68]
	st	%g0, [%sp+92]
	ld	[%fp+68], %o0
	mov	176, %o1
	mov	217, %o2
	sethi	%hi(12733440), %g1
	or	%g1, 768, %o3
	mov	0, %o4
	mov	0, %o5
	call	ata_cmd, 0
	 nop
	mov	%o0, %g1
	st	%g1, [%fp-20]
	ld	[%fp-20], %g1
	cmp	%g1, 0
	be	.LL32
	nop
	mov	-1, %g1
	st	%g1, [%fp-24]
	b	.LL33
	 nop
.LL32:
	st	%g0, [%fp-24]
.LL33:
	ld	[%fp-24], %g1
	mov	%g1, %i0
	ret
	restore
	.size	smart_disable, .-smart_disable
	.align 4
	.global smart_status
	.type	smart_status, #function
	.proc	04
smart_status:
	!#PROLOGUE# 0
	save	%sp, -128, %sp
	!#PROLOGUE# 1
	st	%i0, [%fp+68]
	st	%g0, [%sp+92]
	ld	[%fp+68], %o0
	mov	176, %o1
	mov	218, %o2
	sethi	%hi(12733440), %g1
	or	%g1, 768, %o3
	mov	0, %o4
	mov	0, %o5
	call	ata_cmd, 0
	 nop
	mov	%o0, %g1
	st	%g1, [%fp-20]
	ld	[%fp-20], %g1
	cmp	%g1, 0
	be	.LL35
	nop
	mov	-1, %g1
	st	%g1, [%fp-24]
	b	.LL36
	 nop
.LL35:
	st	%g0, [%fp-24]
.LL36:
	ld	[%fp-24], %g1
	mov	%g1, %i0
	ret
	restore
	.size	smart_status, .-smart_status
	.align 4
	.global smart_status_check
	.type	smart_status_check, #function
	.proc	04
smart_status_check:
	!#PROLOGUE# 0
	save	%sp, -128, %sp
	!#PROLOGUE# 1
	st	%i0, [%fp+68]
	st	%g0, [%sp+92]
	ld	[%fp+68], %o0
	mov	176, %o1
	mov	218, %o2
	sethi	%hi(12733440), %g1
	or	%g1, 768, %o3
	mov	0, %o4
	mov	0, %o5
	call	ata_cmd, 0
	 nop
	mov	%o0, %g1
	st	%g1, [%fp-20]
	ld	[%fp-20], %g1
	cmp	%g1, 0
	be	.LL38
	nop
	mov	-1, %g1
	st	%g1, [%fp-24]
	b	.LL37
	 nop
.LL38:
	st	%g0, [%fp-24]
.LL37:
	ld	[%fp-24], %i0
	ret
	restore
	.size	smart_status_check, .-smart_status_check
	.align 4
	.global smart_auto_offline
	.type	smart_auto_offline, #function
	.proc	04
smart_auto_offline:
	!#PROLOGUE# 0
	save	%sp, -128, %sp
	!#PROLOGUE# 1
	st	%i0, [%fp+68]
	st	%i1, [%fp+72]
	st	%g0, [%sp+92]
	ld	[%fp+68], %o0
	mov	176, %o1
	mov	219, %o2
	sethi	%hi(12733440), %g1
	or	%g1, 768, %o3
	ld	[%fp+72], %o4
	mov	0, %o5
	call	ata_cmd, 0
	 nop
	mov	%o0, %g1
	st	%g1, [%fp-20]
	ld	[%fp-20], %g1
	cmp	%g1, 0
	be	.LL40
	nop
	mov	-1, %g1
	st	%g1, [%fp-24]
	b	.LL41
	 nop
.LL40:
	st	%g0, [%fp-24]
.LL41:
	ld	[%fp-24], %g1
	mov	%g1, %i0
	ret
	restore
	.size	smart_auto_offline, .-smart_auto_offline
	.ident	"GCC: (GNU) 3.4.2"
