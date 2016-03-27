# WiseTagger #
Simple picture tagging tool

## Features ##
* tag autocomplete
* tag implication
* tag replacement and removal
* filesystem-based tag file selection
* picture drag-and-drop support
* image reverse-search (using iqdb.org) with proxy support
* crossplatform, using Qt5 Framework

### Tag File Format ###
Each tag is placed on its own line and  must contain only numbers, letters, «_», «;» (not including quotes).
All whitespace is ignored.

If disallowed character is found, it will be ignored with the rest of the line. This behaviour can be used to make comments, e.g. `// comment`, or `# comment`, etc.

```
# 'unneeded' will be automatically removed
-unneeded

# 'school_uniform' will be automatically replaced with 'seifuku'
seifuku = school_uniform

# 'nekomimi' will be automatically added when 'catgirl' is added
catgirl : nekomimi

# replace and add tags simultaneously
catgirl = animal_ears : nekomimi

# tag lists (comma-separated) may also be used.
some_tag = first_replaced_tag, second_replaced_tag : first_implied_tag, second_implied_tag

# all these tags will be removed
- one, two, three
```

**Note that tags will be presented in autocomplete suggestions in the same order they are in tags file!** Use external tools to sort tags.txt file if needed, e.g.

```
cat original.tags.txt | uniq | sort -o sorted.tags.txt
```

Some imageboard tags will be also replaced with their shorter or longer versions if corresponding option is enabled.
For example, if `Replace imageboard tags` option is enabled, and «yande.re 12345» or «Konachan.com - 67890» tags are present, they will be replaced with «yandere_12345» and «konachan_67890» respectively.
Similarly, if `Restore imageboard tags` option is enabled, «konachan_67890» will be turned back to «Konachan.com - 67890».

### Tag File Selection ###
There are two kinds of tag files: 

* Normal Tag File - Files with suffix `.tags.txt`, e.g. `my.tags.txt`
* Override Tag File - Files with suffix `.tags!.txt`, e.g. `my.tags!.txt`

In both cases prefix can be omitted, leaving only corresponding suffix as file name. Such files starting with `.` are treated as hidden in UNIX-like systems.

To use tag autocompletion place *Normal Tag File* or *Override Tag File* in a directory with your pictures. 

Pictures in subdirectories will pick up tag file in parent directory. 
You can place more *Normal Tag Files* in subdirectories, in this case contents of subdirectory files will be added to contents of parent tag file.

To override tag files for subdirectory, place *Override Tag File* in that directory.

