#global fact_rec
fact_rec:	pushq rbp
		movq rbp,rsp
		subq rsp,16
		cmpq [rbp+16],0
		be base
		pushq [rbp+16]
		subq [rsp],1
		call fact_rec
		popq r0
		mulq r10,[rbp+16]
		ba end
base:	movq r10,1
end:	movq rsp,rbp
		popq rbp
		ret 