module util;

T instanceof(T)(Object o) if(is(T == class)) {
	return cast(T) o;
}
