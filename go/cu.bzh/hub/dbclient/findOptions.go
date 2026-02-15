package dbclient

type FindOptions struct {

	// Limit is the maximum number of documents to return. The default value is 0, which means that all documents matching the
	// filter will be returned. A negative limit specifies that the resulting documents should be returned in a single
	// batch. The default value is 0.
	Limit *int64

	// Sort is a document specifying the order in which documents should be returned. The driver will return an error if the
	// sort parameter is a multi-key map.
	Sort interface{}
}

// SetLimit sets the value for the Limit field.
func (f *FindOptions) SetLimit(i int64) *FindOptions {
	f.Limit = &i
	return f
}

// SetSort sets the value for the Sort field.
func (f *FindOptions) SetSort(sort interface{}) *FindOptions {
	f.Sort = sort
	return f
}

// Regex represents a BSON regex value.
type Regex struct {
	Pattern string
	Options string
}
