#import "Basic";

main :: () {
	a: u8 = 10;
	b: u16 = 50;
	c: u32 = 0x78f;
	d: u64 = 0xfade_deaf_cafe_babe;

	e: s8 = -7;
	f: s16 = 0b101101101;
	g: s32 = -5_069_105;
	h: s64 = 987654321;

	//i : s16 = 80000;
	//j : u32 = -1;

	k := 7;
	print("k is %; type_of(k) is %.\n", k, type_of(k));

	a = cast,no_check(u8) c;
	print("c is %; after casting this to u8, a is now %.\n", c, a);

	my_int : int = 7;
	int_matching := type_of(my_int) == type_of(h);
	print("int_matching: %\n", int_matching);

	l: float32 = 123.005;
	m: float64 = 500.321;

	print("l is of type %, % bytes large.\n", type_of(l), size_of(type_of(l)));
	print("m is of type %, % bytes large.\n", type_of(m), size_of(type_of(m)));

	n:= 1.111;
	o:= 1.11111111111111111;

	print("n is %, of type %.\n", n, type_of(n));
	print("o is %, of type %.\n", o, type_of(o));

	nan32 := 0h7fbf_ffff;
	weird64 := 0h8000_0000_0000_0000;

	print("nan32 is: %\n", nan32);
	print("weird64 is: %\n", weird64);

	x: float32 = 111.0;
	y: float   = x;

	float_matching := type_of(x) == type_of(y);
	print("float_matchingL %\n", float_matching);



}
