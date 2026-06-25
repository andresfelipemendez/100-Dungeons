#import "Basic";

main :: () {
	x: bool;
	x = true;

	print("(step 1) x is %\n", x);

	x = false;

	print("(step 2) x is %\n", x);

	x = !x;

	print("(step 3) x is %\n", x);

	if x {
		print("x was true, so the 'if' block on line % was executed.\n", #location().line_number);
	}

	y := false;

	print("type_of(y) is %\n", type_of(y));

	convert :: (arg:$T) {
		print("% (type %), cast to bool, is %\n", arg, type_of(arg), cast(bool) arg);
	}

	print("\n");

	convert(0);
	convert(1);
	convert(cast(s16) -37);
	convert(69105.33);
	convert("Hello, Sailor!");

	print("\n");

	negative_zero_64 := 0h8000_0000_0000_0000;

	convert(negative_zero_64);

	almost_negative_zero_64 := 0h8000_0000_0000_0001;

	convert(almost_negative_zero_64);

	print("... (We didn't print out enough digits there. If we did, you'd see it's not really 0.)\n");

	if_check :: (arg:$T) {
		b := cast(bool) arg;

		if arg {
			print("b is: %. It had better be true.\n", b);
		} else {
			print("b is: %. I had better be false.\n", b);
		}
	} 

	if_check(0);
	if_check(1);
	if_check(cast(s16) -37);
	if_check(69105.33);
	if_check("Hello, Sailor!");
	if_check(negative_zero_64);
	if_check(almost_negative_zero_64);
}
