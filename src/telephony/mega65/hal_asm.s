	.setcpu "65C02"
	.export _mountd81disk0,_mountd81disk1
	.include "zeropage.inc"
	
.SEGMENT "CODE"
	.p4510
	
cc65_copy_ptr1_string_to_0100:	
    ;; Copy file name
	phy
	ldy #0
@NameCopyLoop:
	lda (ptr1),y
	sta $0100,y
	iny
	cmp #0
	bne @NameCopyLoop
	ply
	rts	

setname_0100:
	;;  Call dos_setname()
	ldy #>$0100
	ldx #<$0100
	lda #$2E                ; dos_setname Hypervisor trap
	sta $D640               ; Do hypervisor trap
	clv                     ; Wasted instruction slot required following hyper trap instruction
	;; XXX Check for error (carry would be clear)
	bcs setname_ok
	lda #$ff
setname_ok:
	rts
	

	.data
dmalist_copysectorbuffer:
	;; Copy $FFD6E00 - $FFD6FFF down to low memory 
	;; MEGA65 Enhanced DMA options
        .byte $0A  ;; Request format is F018A
        .byte $80,$FF ;; Source is $FFxxxxx
        .byte $81,$00 ;; Destination is $FF
        .byte $00  ;; No more options
        ;; F018A DMA list
        ;; (MB offsets get set in routine)
        .byte $00 ;; copy + last request in chain
        .word $0200 ;; size of copy is 512 bytes
        .word $6E00 ;; starting at $6E00
        .byte $0D   ;; of bank $D
copysectorbuffer_destaddr:	
        .word $8000 ;; destination address is $8000
        .byte $00   ;; of bank $0
        .word $0000 ;; modulo (unused)

	.code	

_mountd81disk0:
	;; Get pointer to file name
	sta ptr1+0
	stx ptr1+1	
	jsr cc65_copy_ptr1_string_to_0100
	jsr setname_0100	

	;; Actually call mount
	ldx #0
	lda #$4a
	sta $d640
	clv
	lda #$18
	sta $D640
	clv
	ldx #$00
	rts

_mountd81disk1:
	;; Get pointer to file name
	sta ptr1+0
	stx ptr1+1	
	jsr cc65_copy_ptr1_string_to_0100
	jsr setname_0100	

	;; Actually call mount
	ldx #1
	lda #$4a
	sta $d640
	clv
	lda #$18
	sta $D640
	clv
	ldx #$00
	rts
	
