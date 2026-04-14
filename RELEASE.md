RELEASE_TYPE: patch

Fix the compositional fallback path of `vectors` and `dictionaries` to
correctly honor uniqueness when element/key generators lack a schema.
Previously the fallback for `vectors({.unique = true})` produced a
potentially non-unique vector, and `dictionaries` could loop for an
unbounded number of attempts when the key generator repeatedly returned
duplicates. Both now cap attempts and reject the test case via
`hegel::internal::assume` when the generator cannot satisfy the
requested size with unique values.
