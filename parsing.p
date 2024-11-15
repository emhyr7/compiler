{
	position: (0, 0);

	foo:  1;
	bar: -1;
	
	a: foo = 7;
	b: bar : 21;
	c :: 14;

	a ? b;
	a ? b ! c;
	a + b * c;
	!(a + b) * c;
	!(a + b) * a ? b ! c;
	!(a + b) * (a ? b ! c);
	{
		!a.b;
		"string\t";
		21;
	}
	7.14;

	.label

	.sub: (a: foo, b: foo) r: bar {
		r = a - b;
		return;
	}

	r := sub(0, 0);

	array: (uint8.. 32);
	array2: (uint8.. 32 + 32) = (1, 2, 3, 4, 5, 6, 7);
}
