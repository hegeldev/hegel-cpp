RELEASE_TYPE: patch

Fix the compositional fallback path of `vectors` and `maps` to
correctly honor uniqueness when element/key generators lack a schema.
Previously the fallback for `vectors({.unique = true})` produced a
potentially non-unique vector, and `maps` could loop for an
unbounded number of attempts when the key generator repeatedly returned
duplicates. Both now cap attempts and reject the test case via
`tc.assume()` when the generator cannot satisfy the requested size with
unique values.

Update and build our documentation to https://hegel.dev/.
