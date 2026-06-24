#import "Basic";

a_variable : float = 37.0;
b_variable : float;
c_variable : = 111.0;

d_variable : float : -123.45;

main :: () {
	print("A is of type %, value %.\n", type_of(a_variable), a_variable);
	print("B is of type %, value %.\n", type_of(b_variable), b_variable);
	print("C is of type %, value %.\n", type_of(c_variable), c_variable);
	print("D is of type %, value %.\n", type_of(d_variable), d_variable);
}
