#global main
#extern fact
#extern fact_rec
a:	i64 17
b:	i64 5
// 15! * 5! = 156920924160000
// 17! * 5! = 42682491371520000
main:	pushq rbp
		movq rbp, rsp
		subq rsp, 24
		movq [rbp-16],[b]
		movq [rbp-8],[a]
		pushq [rbp-16]

		lea r5,[fact]
		call r5
		//call fact	//el return esta en r10
		movq [rbp-24],r10
		pushq [rbp-8]
		call fact_rec
		mulq r10,[rbp-24]
		movq rsp,rbp
		popq rbp
		ret