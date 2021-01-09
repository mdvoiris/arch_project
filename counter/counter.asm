		add $r2, $zero, $imm, 127		# num_of_writes = 127
LOOP:	ll $r3, $zero, zero, 0			# load linked cur $r3
		add $r3, $r3, $imm, 1			# $r3 += 1
		sc $r3, $zero, $zero, 0			# store conditional $r3
		beq $imm, $r3, $zero, LOOP		# if store conditional faild goto FAILED
		add $zero, $zero, $zero, 0		# does nothing, to fill the delay slot
		bne $imm, $r2, $zero, LOOP		# if (num_of_writes != 0) continue LOOP 
		sub $r2, $r2, $imm, 1			# num_of_writes -=1 (delay slot)
		lw $r3, $zero, $zero, 0			# update main memory
		halt $zero, $zero, $zero, 0		# else finished

