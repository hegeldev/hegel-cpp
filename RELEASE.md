RELEASE_TYPE: patch

This patch fixes `text()` with an `alphabet`, which could generate characters outside the alphabet ([#146](https://github.com/hegeldev/hegel-cpp/issues/146)). With some standard libraries an empty category list reached the engine as a null pointer, which the engine reads as "no restriction".
