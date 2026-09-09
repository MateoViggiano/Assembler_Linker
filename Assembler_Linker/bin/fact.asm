#global fact
fact:	pushq rbp
		movq rbp,rsp
		subq rsp,16
		movq [rbp-8],1
		movq [rbp-16],1
loop:	cmpq [rbp-16],[rbp+16]
		bgt end 
		mulq [rbp-8],[rbp-16]
		addq [rbp-16],1
		ba loop
end:	movq r10,[rbp-8]
		movq rsp,rbp
		popq rbp
		ret
