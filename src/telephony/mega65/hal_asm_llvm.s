	.global mountd81disk0,mountd81disk1, irq_wait_animation
	.type  mountd81disk0,@function
	.type  mountd81disk1,@function
	.type  irq_wait_animation,@function

irq_wait_animation:

	;; Frame counter for delays
	;; XXX - Must match SHARED_ADDR in shshate.h
	inc $033C
	
	;; Move weight down the screen
	inc $d005
	inc $d005

	;; Check if it's near the bottom of the screen
	lda $d005
	cmp #$fe
	bcc not_at_bottom

	;; It was near the bottom of the screen
	
	;;  Move wait back to top of screen
	lda #$18
	sta $d005
	;; and randomise X position somewhat
	lda $d012
	sta $d004
	
not_at_bottom:
	jmp $ea31
	
llvm_copy_arg1_to_0100:	
    ;; Copy file name
	phy	
	
	ldy #0
NameCopyLoop:
	lda (__rc2),y
	sta $0100,y
	iny
	cmp #0
	bne NameCopyLoop
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

mountd81disk0:
	;; Mount on drive 0
	ldx #0
	phx
do_the_mount:
	;; Get pointer to file name
	jsr llvm_copy_arg1_to_0100
	jsr setname_0100	
	
	;; Do the actual mount
	plx
	lda #$4a
	sta $d640
	clv
	ldx #$00
	bcc fail
success:
	lda #$01
	rts

fail:
	;; Return HYPPO DOS error code
	lda #$38
	sta $d640
	nop
	rts

mountd81disk1:
	ldx #1
	phx
	jmp do_the_mount
	

	;;  Ensure Z is cleared on entry	
	.section .init.000,"ax",@progbits
	ldz #0			
	cld			; Because I'm really paranoid
