Creating input PNG files
========================

`bgra-iphone.png` is `rgba.png` converted to a CgBI format (an Apple-specific
extension to PNG, using BGRA and premultiplied alpha). Details here:
http://iphonedevwiki.net/index.php/CgBI_file_format

It's converted using [pincrush](https://github.com/DHowett/pincrush),
unfortunately the current version (2f61dfc85d84c89da6f6d943d3192c71077fd3cd)
doesn't compile out of the box on Linux and you need to apply the following
patches:

In the root directory:

```diff
diff --git a/framework b/framework
--- a/framework
+++ b/framework
@@ -1 +1 @@
-Subproject commit 381546a1573949dba8d083e4d3c90223d18f2098
+Subproject commit 381546a1573949dba8d083e4d3c90223d18f2098-dirty
diff --git a/pincrush.c b/pincrush.c
index a6bb43c..c9e0a87 100644
--- a/pincrush.c
+++ b/pincrush.c
@@ -4,6 +4,7 @@
 #include <stdbool.h>
 #include <unistd.h>
 #include <png.h>
+#include <getopt.h>

 #define RDERR(...) { debuglog(LEVEL_ERROR, infilename, __VA_ARGS__); goto out; }
 #define WRERR(...) { debuglog(LEVEL_ERROR, inplace?infilename:outfilename, __VA_ARGS__); goto out; }
```

In the framework submodule:

```diff
diff --git a/makefiles/instance/subproject.mk b/makefiles/instance/subproject.mk
index bcfa200..bf8e913 100644
--- a/makefiles/instance/subproject.mk
+++ b/makefiles/instance/subproject.mk
@@ -17,4 +17,4 @@ internal-subproject-compile: $(FW_OBJ_DIR)/$(FW_SUBPROJECT_PRODUCT)
 endif

 $(FW_OBJ_DIR)/$(FW_SUBPROJECT_PRODUCT): $(OBJ_FILES_TO_LINK)
-	$(ECHO_LINKING)$(TARGET_CXX) -nostdlib -r -d $(ADDITIONAL_LDFLAGS) $(TARGET_LDFLAGS) $(LDFLAGS) -o $@ $^$(ECHO_END)
+	$(ECHO_LINKING)$(TARGET_CXX) -nostdlib -r $(TARGET_LDFLAGS) $(LDFLAGS) -o $@ $^$(ECHO_END)
```

Then build the project using

```sh
make target=native
```

Converting the PNGs is done like this (note that only the `-i` option works,
having a separate output file crashes horribly in various places and it's not
worth investigating):

```sh
/path/to/pincrush/obj/linux/pincrush -v -i <file>.png
mv <file>.png <file>-iphone.png
git checkout <file>.png
```
