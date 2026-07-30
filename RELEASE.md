RELEASE_TYPE: patch

This patch fixes a bug in the `HEGEL_REQUIRE_EQUAL` renderability check. 
A `std::vector`, `std::set`, `std::map`, `std::array`, `std::optional`, 
`std::pair`, `std::tuple`, `std::variant`, or struct is now rejected at compile
time when its element or field type is one Hegel cannot render. Before, only the
outermost type was checked, so a comparison of two `std::vector<T>` values whose
`T` had no rendering compiled and always passed. A struct that inherits is 
rejected now as well.
