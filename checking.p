{
	# unsigned integers
	uint8  :: 0xff;
	uint16 :: 0xffff;
	uint32 :: 0xffffffff;
	uint64 :: 0xffffffffffffffff;

	# signed integers
	sint8  :: -(uint8  >> 1);
	sint16 :: -(uint16 >> 1);
	sint32 :: -(uint32 >> 1);
	sint64 :: -(uint64 >> 1);

	# floating-points
	float32 :: 0.0;
	float64 :: 0.0;

	# void
	void :: ();

	# pointer
	pointer :: @void;

	# array
	u8x64 :: (64** uint8);

	# set / enum
	day :: [monday, tuesday, wednesday, thursday, friday, saturday, sunday];
	failure :: [absent, overflow];
	failure_messages: (failure** @uint8) : (.'absent = "absent", .failure.overflow = "overflow");

	# record
	f64x2 :: (x: float64, y: float64);

	# union
	word :: (value: uint16) | (high: uint8, low: uint8);

	# alias
	position :: f64x2;

	# routine
	callback :: (data: @void) void;

	# initializations
	uninitialized: position;
	initialized: position = (7.0, 7.0);
	default_initialized := position;
}
