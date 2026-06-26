#import "Basic";

arrays_part_1 :: () {
	
	print("Part 1:\n\n");

	x: [8] int;

	x[3] = 9;
	x[4] = 16;
	x[5] = x[3]+x[4];
	print("x[5] is: %\n", x[5]);

	n := 37;
	// y := x[n];
	// y := x[44];

	print("\n");	
	for index: 0..7 {
		print("x[%] == %\n", index, x[index]);
	}
	print("\n");
	
	my_strings: [5] string; 

	my_strings[1] = "frotz";
	my_strings[3] = "xyzzy";
	my_strings[4] = "gondar";

	for s, i: my_strings {
		print("my_strings[%] is: '%'\n", i, s);
	}
}

arrays_part_2 :: () {
	print("\n");
	print("Part 2\n\n");

	a: [8] float;
	b: [9] float;

	print("type_of(a) is %.\n", type_of(a));
	print("type_of(b) is %.\n", type_of(b));

	print("type_of(a) == type_of(b): %.\n", type_of(a) == type_of(b));

	c: [] float;

	for i: 0..7 a[i] = cast(float) i;

	c = a;

	print("c[5] is: %\n", c[5]);

	// array views are bounds-checked, although at run time
	// c[9] = 9;

	print("a is % bytes large.\n", size_of(type_of(a)));
	print("c is % bytes large.\n", size_of(type_of(c)));
	print("\n");

	print_sum :: (name: string, array: [] float) {
		sum: float;
		for value: array sum += value;

		print("The sum of elements in '%' is: %\n", name, sum);
	}

	print_sum("a", a);
	print_sum("b", b);
	print_sum("c", c);

	c.count = 5;
	for value, index: c {
		print("c[%] == %\n", index, value);
	}

	c[1] = 42;
	print("after writing to c, a[1] == %\n", a[1]);

	print("\n");
	print("Before pointer bump:\n");
	// for value
}

resizable_arrays :: () {

}

recap :: () {

}

main :: () {
	print("\n");
	print("-- Part 1 --");
	arrays_part_1();

	print("\n");
	print("-- Part 2 --");
	arrays_part_2();

	print("\n");
	print("-- Resizable Arrays --");
	resizable_arrays();

	print("\n");
	print("-- Recap --");
	recap();
}
