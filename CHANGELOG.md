## 2.2.1 (Current)

- Added support to handle script elements in html.

- Added support for position from start for the sax parser.

## 2.2.0

- Added the SAX convert_special option to the default options.

- Added the SAX smart option to the default options.

- Other SAX options are now taken from the defaults if not specified.

## 2.1.8

- Fixed a bug that caused all input to be read before parsing with the sax
  parser and an IO.pipe.

## 2.1.7

- Empty elements such as <foo></foo> are now called back with empty text.

- Fixed GC problem that occurs with the new GC in Ruby 2.2 that garbage
  collects Symbols.

## 2.1.6

- Update licenses. No other changes.

## 2.1.5

- Fixed symbol intern problem with Ruby 2.2.0. Symbols are not dynamic unless
  rb_intern(). There does not seem to be a way to force symbols created with
  encoding to be pinned.

## 2.1.4

- Fixed bug where the parser always started at the first position in a stringio
  instead of the current position.

## 2.1.3

- Added check for @attributes being nil. Reported by and proposed fix by Elana.

## 2.1.2

- Added skip option to parsing. This allows white space to be collapsed in two
  different ways.

- Added respond_to? method for easy access method checking.

## 2.1.1

- Worked around a module reset and clear that occurs on some Rubies.

## 2.1.0

- Thanks to jfontan Ox now includes support for XMLRPC.

## 2.0.12

- Fixed problem compiling with latest version of Rubinius.

## 2.0.11

- Added support for BigDecimals in :object mode.

## 2.0.10

- Small fix to not create an empty element from a closed element when using locate().

- Fixed to keep objects from being garbages collected in Ruby 2.x.

## 2.0.9

- Fixed bug that did not allow ISO-8859-1 characters and caused a crash.

## 2.0.8

- Allow single quoted strings in all modes.

## 2.0.7

- Fixed DOCTYPE parsing to handle nested '>' characters.

## 2.0.6

- Fixed bug in special character decoding that chopped of text.

- Limit depth on dump to 1000 to avoid core dump on circular references if the user does not specify circular.

- Handles dumping non-string values for attributes correctly by converting the value to a string.

## 2.0.5

 - Better support for special character encoding with 1.8.7.

## 2.0.4

- Fixed SAX parser handling of &#nnnn; encoded characters.

## 2.0.3

- Fixed excessive memory allocation issue for very large file parsing (half a gig).

## 2.0.2

- Fixed buffer sliding window off by 1 error in the SAX parser.

## 2.0.1

- Added an attrs_done callback to the sax parser that will be called when all
   attributes for an element have been read.

- Fixed bug in SAX parser where raising an exception in the handler routines
   would not cleanup. The test put together by griffinmyers was a huge help.

- Reduced stack use in a several places to improve fiber support.

- Changed exception handling to assure proper cleanup with new stack minimizing.

## 2.0.0

- The SAX parser went through a significant re-write. The options have changed. It is now 15% faster on large files and
   much better at recovering from errors. So much so that the tolerant option was removed and is now the default and
   only behavior. A smart option was added however. The smart option recognizes a file as an HTML file and will apply a
   simple set of validation rules that allow the HTML to be parsed more reasonably. Errors will cause callbacks but the
   parsing continues with the best guess as to how to recover. Rubymaniac has helped with testing and prompted the
   rewrite to support parsing HTML pages.

- HTML is now supported with the SAX parser. The parser knows some tags like \<br\> or \<img\> do not have to be
   closed. Other hints as to how to parse and when to raise errors are also included. The parser does it's best to
   continue parsing even after errors.

- Added symbolize option to the sax parser. This option, if set to false will use strings instead of symbols for
   element and attribute names.

- A contrib directory was added for people to submit useful bits of code that can be used with Ox. The first
   contributor is Notezen with a nice way of building XML.

## 1.9.4

- SAX tolerant mode handle multiple elements in a document better.

## 1.9.3

- mcarpenter fixed a compile problem with Cygwin.

- Now more tolerant when the :effort is set to :tolerant. Ox will let all sorts
   of errors typical in HTML documents pass. The result may not be perfect but
   at least parsed results are returned.

   - Attribute values need not be quoted or they can be quoted with single
     quotes or there can be no =value are all.

   - Elements not terminated will be terminated by the next element
     termination. This effect goes up until a match is found on the element
     name.

- SAX parser also given a :tolerant option with the same tolerance as the string parser.

## 1.9.2

- Fixed bug in the sax element name check that cause a memory write error.

## 1.9.1

- Fixed the line numbers to be the start of the elements in the sax parser.

## 1.9.0

- Added a new feature to Ox::Element.locate() that allows filtering by node Class.

- Added feature to the Sax parser. If @line is defined in the handler it is set to the line number of the xml file
  before making callbacks. The same goes for @column but it is updated with the column.

## 1.8.9

- Fixed bug in element start and end name checking.

## 1.8.8

- Fixed bug in check for open and close element names matching.

## 1.8.7

- Added a correct check for element open and close names.

- Changed raised Exceptions to customer classes that inherit from StandardError.

- Fixed a few minor bugs.

## 1.8.6

- Removed broken check for matching start and end element names in SAX mode. The names are still included in the
  handler callbacks so the user can perform the check is desired.

## 1.8.5

- added encoding support for JRuby where possible when in 1.9 mode.

## 1.8.4

- Applied patch by mcarpenter to fix solaris issues with build and remaining undefined @nodes.

## 1.8.3

- Sax parser now honors encoding specification in the xml prolog correctly.

## 1.8.2

- Ox::Element.locate no longer raises and exception if there are no child nodes.

- Dumping an XML document no longer puts a carriage return after processing instructions.

## 1.8.1

- Fixed bug that caused a crash when an invalid xml with two elements and no <?xml?> was parsed. (issue #28)

- Modified the SAX parser to not strip white space from the start of string content.

## 1.8.0

- Added more complete support for processing instructions in both the generic parser and in the sax parser. This change includes and additional sax handler callback for the end of the instruction processing.

## 1.7.1

- Pulled in sharpyfox's changes to make Ox with with Windows. (issue #24)

- Fixed bug that ignored white space only text elements. (issue #26)

## 1.7.0

- Added support for BOM in the SAX parser.

## 1.6.9

- Added support for BOM. They are honored for and handled correctly for UTF-8. Others cause encoding issues with Ruby or raise an error as others are not ASCII compatible..

## 1.6.8

- Changed extconf.rb to use RUBY_PLATFORM.

## 1.6.7

- Now uses the encoding of the imput XML as the default encoding for the parsed output if the default options encoding is not set and the encoding is not set in the XML file prolog.

## 1.6.5

- Special character handling now supports UCS-2 and UCS-4 Unicode characters as well as UTF-8 characters.

## 1.6.4

- Special character handling has been improved. Both hex and base 10 numeric values are allowed up to a 64 bit number
  for really long UTF-8 characters.

## 1.6.3

- Fixed compatibility issues with Linux (Ubuntu) mostly related to pointer sizes.

## 1.6.2

- Added check for Solaris and Linux builds to not use the timezone member of time struct (struct tm).

## 1.6.1

- Added check for Solaris builds to not use the timezone member of time struct (struct tm).
