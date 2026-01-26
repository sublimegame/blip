package scyllaclient

type errWrongKeyspace struct {
	expected string
	got      string
}

func (e *errWrongKeyspace) Error() string {
	return "scyllaDB client is for wrong keyspace, expected " + e.expected + ", got " + e.got
}
