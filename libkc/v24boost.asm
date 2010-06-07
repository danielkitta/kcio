; Copyright (c) 2010  Daniel Elstner <daniel.kitta@gmail.com>
;
; This file is part of KC-I/O.
;
; KC-I/O is free software: you can redistribute it and/or modify it
; under the terms of the GNU General Public License as published by the
; Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; KC-I/O is distributed in the hope that it will be useful, but
; WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
; See the GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License along
; with this program.  If not, see <http://www.gnu.org/licenses/>.

; V.24 receive program for the 19200 Baud boost mode of kcsend.
; This program is written into the cassette tape buffer and then
; executed to receive the bulk of the data at a rate of roughly
; 19200 Baud instead of the 1200 Baud default.  After each block
; of 128 Byte, a checksum is sent back to the sender to provide
; rudimentary integrity verification.

; Command to assemble and output hex dump:
; z80asm -o - libkc/v24boost.asm | hexdump -e '8/1 "0x%.2X, " "\n"'

tapbuf:	equ	0B700h		; address of cassette tape buffer
sioc:	equ	0Bh
siod:	equ	09h
ctc:	equ	0Dh
dtroff:	equ	01101010b	; DTR off, 8 bit, transmit enable, RTS on
dtron:	equ	10000000b + dtroff

	db	54h		; 'T' command
	dw	tapbuf		; address of cassette tape buffer
	dw	pend - tapbuf	; program length

	org	tapbuf

	ld	bc,0100h * v24il + 3	; 18400 Baud polling mode
	call	v24ini

	call	cin
	ld	c,a		; bc = start address
	call	cin
	ld	b,a
	call	cin
	ld	l,a		; hl = byte count
	call	cin
	ld	h,a
	jr	lcond

loop:	call	cin
	ld	(bc),a
	inc	bc
	add	a,d		; checksum
	ld	d,a
	dec	e
	jr	nz,loop

cout:	in	a,(sioc)
	and	00000100b	; transmit buffer empty?
	jr	z,cout
	ld	a,d
	out	(siod),a	; send byte

lcond:	ld	de,0080h	; checksum = 0, byte count = block size
	xor	a		; cy = 0
	sbc	hl,de
	jr	nc,loop
	add	hl,de
	ld	e,l		; e = remainder
	ld	l,a		; l = 0
	cp	e		; remainder = 0?
	jr	c,loop

drain:	ld	a,1
	di
	out	(sioc),a
	in	a,(sioc)
	ei
	rrca			; all sent?
	jr	nc,drain

	ld	bc,0100h * v24rl + 46	; 1200 Baud interrupt mode
	; b = I/O table length
	; c = CTC time constant
v24ini:	ld	hl,v24tab
	ld	a,01000111b	; reset counter, time constant follows
	di
	out	(ctc),a
	ld	a,c
	out	(ctc),a
	ld	c,sioc
	otir
	ei
	ret

cin0:	ld	a,dtron		; signal ready to sender
	out	(sioc),a
	ei
cin:	in	a,(sioc)
	rrca			; data waiting?
	ld	a,5		; select WR5
	di
	out	(sioc),a
	jr	nc,cin0
cin1:	ld	a,dtroff	; signal busy to sender
	out	(sioc),a
	ei
	in	a,(siod)	; read received byte
	ret

v24tab:	db	00011000b	; channel reset
	db	4,01000100b	; x16 clock, 1 stop bit, no parity
	db	3,11100001b	; 8 bit, auto enables, receive enable
	db	5,dtron
v24il:	equ	$ - v24tab
	db	11h,00011000b	; interrupt reset and enable
v24rl:	equ	$ - v24tab

pend:	end
