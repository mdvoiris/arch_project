add $r13 $zero $imm 1//r13=1           explanations for the main registers in the Assembly code:
add $r2 $zero $imm F/r2=15             $r9- counting iterations till needs to update one element in the result matrix
add $r14 $zero $imm 10//r14=16         $r3- an element in the matrix wich we scan row after row  $r4- an index in this matrix
add $r12 $zero $imm 40//r12=256       $r5-  an element in the matrix wich we scan column after column $r6-an index in this matrix
lw $r3 $r4 $imm C0//for                $r11- counting how many elements were updated in the result matrix in each row
lw $r5 $r6 $imm 100                    $r8- an element in the resault matrix  $r10-an index in this matrix
mul $r7 $r5 $r3 0//
add $r4 $r4 $r13 0//moving the index
add $r6 $r6 $imm 10//moving the index
add $r8 $r8 $r7 0
beq $imm $r9 $r2 F// goto update1
add $r9 $r9 $r13 0// when r9=16 go to update1 
beq $imm $zero $zero 4//goto for
add $zero $zero $zero 0
sw $r8 $r10 $imm 2C0//update1
sub $r4 $r4 $imm 10//back to start of row
sub $r6 $r6 $imm FF //NEXT column
add $r9 $zero $zero 0// when r9=0 go to update1 
add $r11 $r11 $r13 0
add r8 $zero $zero 0
beq $imm $r10 $r12 1F//goto halt
add $r10 $r10 $r13 0
beq $imm $r11 $r14 1A//goto update2
add $zero $zero $zero 0
beq $imm $zero $zero 4//goto for
add $zero $zero $zero 0
add $r6 $zero $zero 0//update2, go to start of matrix
add $r4 $r4 $imm 10// move to next row 
add r11 zero zero 0
beq $imm zero $zero 4// GOTO for
add $r11 $zero $zero 0// r11=0
lw $r5 $zero $imm 3FF // inforce miss conflict to update main memory 
halt $zero $zero $zero 0//halt
