# WiseTagger #

Simple picture filename-tagging tool

## Features ##

* tag autocomplete
* tag replacement (including common booru's tags replacement)
* tag removal
* related tags insertion
* per-directory tags.txt files (in config.json)
* picture drag-and-drop support
* crossplatform, uses Qt (should compile and run wherever Qt runs, haven't tested it yet)

### Tags File Format ###

Each tag is placed on its own line and  must contain only numbers, letters, «_», «;» (not including quotes). 
All whitespace is ignored.

If disallowed character is found, it will be ignored with the rest of the line. This behaviour can be used to make comments, e.g. "// comment", or "# comment", etc.


```
// 'tag' will be automatically removed
-tag

// 'other_tag' will be automatically replaced with 'tag'
tag = other_tag

// 'related_tag' will be automatically added when 'tag' is added
tag : related_tag

// replace and add tags simultaneously
tag = replaced_tag : related_tag

// tag lists (comma-separated) may be used.
tag = replaced_tag_1, replaced_tag_2 : related_tag_1, related_tag_2

// all these tags will be removed
- tag_1, tag_2, third_tag
```

**Note that tags will be presented in autocomplete suggestions in the same order they are in tags.txt!** Use external tools to sort tags.txt file if needed, e.g.

```
cat tags.txt | uniq | sort -o tags.sorted.txt
```

Some booru's tags will be also replaced regardless of tags.txt contents, this is hardcoded as of now. 
For example, if «yande.re 12345» or «Konachan.com - 67890» is found in filename, they will be replaced with «yandere_12345» and «konachan_67890» respectively.


### Configuration ###
To use different tags.txt files for different folders, add new entry to config.json file with that directory's path and corresponding tags.txt file path. Example:
```
[
    {
        "directory": "*",
        "tagfile": "tags.txt"
    },
    {
        "directory": "/some/path/to/folder/with/pics",
        "tagfile": "/some/path/to/tags.txt"
    }
]
```

**Only / path separator is allowed!**
Relative paths are not really supported yet, they will be resolved from executable's directory.

«*» directory denotes tags file used with all other directories.
	





