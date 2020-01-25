#!/usr/bin/env ruby
# encoding: utf-8

# Ubuntu does not accept arguments to ruby when called using env. To get warnings to show up the -w options is
# required. That can be set in the RUBYOPT environment variable.
# export RUBYOPT=-w

$VERBOSE = true

$: << File.join(File.dirname(__FILE__), "../../lib")
$: << File.join(File.dirname(__FILE__), "../../ext")
$: << File.join(File.dirname(__FILE__), ".")

require 'stringio'
require 'bigdecimal'
require 'test/unit'
require 'optparse'

require 'ox'

require 'helpers'
require 'smart_test'

opts = OptionParser.new
opts.on("-h", "--help", "Show this display")                { puts opts; Process.exit!(0) }
opts.parse(ARGV)

$ox_sax_options = {
  :encoding=>nil,
  :indent=>2,
  :trace=>0,
  :with_dtd=>false,
  :with_xml=>false,
  :with_instructions=>false,
  :circular=>false,
  :xsd_date=>false,
  :mode=>:object,
  :symbolize_keys=>true,
  :skip=>:skip_return,
  :smart=>false,
  :convert_special=>true,
  :effort=>:strict,
  :invalid_replace=>'',
  :strip_namespace=>false
}

class SaxBaseTest < ::Test::Unit::TestCase
  include SaxTestHelpers

  def test_sax_io_pipe
    Ox::default_options = $ox_sax_options
    handler = AllSax.new()
    input,w = IO.pipe
    w << %{<top/>}
    w.close
    Ox.sax_parse(handler, input)
    assert_equal([[:start_element, :top],
                  [:end_element, :top]], handler.calls)
  end

  def test_sax_file
    Ox::default_options = $ox_sax_options
    handler = AllSax.new()
    input = File.open(File.join(File.dirname(__FILE__), 'basic.xml'))
    Ox.sax_parse(handler, input)
    input.close
    assert_equal([[:start_element, :top],
                  [:end_element, :top]], handler.calls)
  end

  def test_sax_file_line_col
    Ox::default_options = $ox_sax_options
    handler = LineColSax.new()
    input = File.open(File.join(File.dirname(__FILE__), 'trilevel.xml'))
    Ox.sax_parse(handler, input)
    input.close
    assert_equal([[:instruct, "xml", 1, 1, 1],
                  [:attr, :version, "1.0", 15, 1, 15],
                  [:attrs_done, 15, 1, 15],
                  [:end_instruct, "xml", 20, 1, 20],
                  [:start_element, :top, 23, 2, 1],
                  [:attrs_done, 23, 2, 1],
                  [:start_element, :child, 31, 3, 3],
                  [:attrs_done, 31, 3, 3],
                  [:start_element, :grandchild, 43, 4, 5],
                  [:attrs_done, 43, 4, 5],
                  [:end_element, :grandchild, 55, 4, 17],
                  [:end_element, :child, 59, 5, 3],
                  [:end_element, :top, 68, 6, 1]], handler.calls)
  end

  def test_sax_io_file
    Ox::default_options = $ox_sax_options
    handler = AllSax.new()
    input = IO.open(IO.sysopen(File.join(File.dirname(__FILE__), 'basic.xml')))
    Ox.sax_parse(handler, input)
    input.close
    assert_equal([[:start_element, :top],
                  [:end_element, :top]], handler.calls)
  end

  def test_sax_instruct_simple
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml?>}, [[:instruct, 'xml'],
                               [:end_instruct, 'xml']])
  end

  def test_sax_instruct_blank
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml?>}, [], StartSax)
  end

  def test_sax_instruct_attrs
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml version="1.0" encoding="UTF-8"?>},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:attr, :encoding, "UTF-8"],
                   [:end_instruct, 'xml']])
  end

  def test_sax_instruct_noattrs
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?bad something?>},
                  [[:instruct, 'bad'],
                   [:text, "something"],
                   [:end_instruct, 'bad']])
  end

  def test_sax_instruct_loose
    Ox::default_options = $ox_sax_options
    parse_compare(%{<? xml
version = "1.0"
encoding = "UTF-8" ?>},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:attr, :encoding, "UTF-8"],
                   [:end_instruct, 'xml']])
  end

  def test_sax_instruct_pi
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?pro cat="quick"?>
<top>
  <str>This is a <?attrs dog="big"?> string.</str>
  <?content dog is big?>
</top>
},
                  [[:instruct, 'pro'],
                   [:attr, :cat, "quick"],
                   [:end_instruct, 'pro'],
                   [:start_element, :top],
                   [:start_element, :str],
                   [:text, "This is a "],
                   [:instruct, "attrs"],
                   [:attr, :dog, "big"],
                   [:end_instruct, "attrs"],
                   [:text, " string."],
                   [:end_element, :str],
                   [:instruct, "content"],
                   [:text, "dog is big"],
                   [:end_instruct, "content"],
                   [:end_element, :top]])
  end

  def test_sax_instruct_no_term
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml?
<top/>},
                  [[:instruct, "xml"],
                   [:error, "Not Terminated: instruction not terminated", 1, 6],
                   [:end_instruct, "xml"],
                   [:start_element, :top],
                   [:end_element, :top]])
  end

  def test_sax_element_simple
    Ox::default_options = $ox_sax_options
    parse_compare(%{<top/>},
                  [[:start_element, :top],
                   [:end_element, :top]])
  end

  def test_sax_element_attrs
    Ox::default_options = $ox_sax_options
    parse_compare(%{<top x="57" y='42' z=33/>},
                  [[:start_element, :top],
                   [:attr, :x, "57"],
                   [:attr, :y, "42"],
                   [:error, "Unexpected Character: attribute value not in quotes", 1, 22],
                   [:attr, :z, "33/"],
                   [:error, "Start End Mismatch: element 'top' not closed", 1, 25],
                   [:end_element, :top]])
  end
  def test_sax_element_start_end
    Ox::default_options = $ox_sax_options
    parse_compare(%{<top></top>},
                  [[:start_element, :top],
                   [:text, ""],
                   [:end_element, :top]])
  end

  def test_sax_two_top
    Ox::default_options = $ox_sax_options
    parse_compare(%{<top/><top/>},
                  [[:start_element, :top],
                   [:end_element, :top],
                   [:error, "Out of Order: multiple top level elements", 1, 7],
                   [:start_element, :top],
                   [:end_element, :top]])
  end

  def test_sax_nested_elements
    Ox::default_options = $ox_sax_options
    parse_compare(%{<top>
  <child>
    <grandchild/>
  </child >
</top>
},
                  [[:start_element, :top],
                   [:start_element, :child],
                   [:start_element, :grandchild],
                   [:end_element, :grandchild],
                   [:end_element, :child],
                   [:end_element, :top],
                  ])
  end

  def test_sax_elements_case
    Ox::default_options = $ox_sax_options
    Ox::default_options = { :smart => true, :skip => :skip_white }
    parse_compare(%{<top>
  <Child>
    <grandchild/>
  </child >
</TOP>
},
                  [[:start_element, :top],
                   [:start_element, :Child],
                   [:start_element, :grandchild],
                   [:end_element, :grandchild],
                   [:end_element, :Child],
                   [:end_element, :top],
                  ], AllSax, { :smart => true })
  end

  def test_sax_nested1
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml version="1.0"?>
<top>
  <child>
    <grandchild/>
  </child>
</top>
},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:end_instruct, 'xml'],
                   [:start_element, :top],
                   [:start_element, :child],
                   [:start_element, :grandchild],
                   [:end_element, :grandchild],
                   [:end_element, :child],
                   [:end_element, :top],
                  ])
  end

  def test_sax_line_col
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml version="1.0"?>
<!DOCTYPE top PUBLIC "top.dtd">
<top attr="one">
  <!-- family -->
  <child>
    <grandchild/>
    <![CDATA[see data.]]>
  </child>Some text.
</top>
<second>
  <inside/>
</second>
},
                  [[:instruct, 'xml', 1, 1, 1],
                   [:attr, :version, "1.0", 15, 1, 15],
                   [:attrs_done, 15, 1, 15],
                   [:end_instruct, 'xml', 20, 1, 20],
                   [:doctype, " top PUBLIC \"top.dtd\"", 23, 2, 1],
                   [:start_element, :top, 55, 3, 1],
                   [:attr, :attr, "one", 65, 3, 11],
                   [:attrs_done, 65, 3, 11],
                   [:comment, " family ", 74, 4, 3],
                   [:start_element, :child, 92, 5, 3],
                   [:attrs_done, 92, 5, 3],
                   [:start_element, :grandchild, 104, 6, 5],
                   [:attrs_done, 104, 6, 5],
                   [:end_element, :grandchild, 116, 6, 17],
                   [:cdata, "see data.", 122, 7, 5],
                   [:end_element, :child, 146, 8, 3],
                   [:text, "Some text.\n", 154, 8, 10],
                   [:end_element, :top, 165, 9, 1],
                   [:error, "Out of Order: multiple top level elements", 10, 1],
                   [:start_element, :second, 172, 10, 1],
                   [:attrs_done, 172, 10, 1],
                   [:start_element, :inside, 183, 11, 3],
                   [:attrs_done, 183, 11, 3],
                   [:end_element, :inside, 191, 11, 11],
                   [:end_element, :second, 193, 12, 1],
                  ], LineColSax)
  end

  def test_sax_element_name_mismatch
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml version="1.0"?>
<top>
  <child>
    <grandchild/>
  </parent>
</top>},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:end_instruct, "xml"],
                   [:start_element, :top],
                   [:start_element, :child],
                   [:start_element, :grandchild],
                   [:end_element, :grandchild],
                   [:error, "Start End Mismatch: element 'parent' closed but not opened", 5, 3],
                   [:start_element, :parent],
                   [:end_element, :parent],
                   [:error, "Start End Mismatch: element 'top' close does not match 'child' open", 6, 1],
                   [:end_element, :child],
                   [:end_element, :top],
                  ])
  end

  def test_sax_nested
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml version="1.0"?>
<top>
  <child>
    <grandchild/>
  </child>
  <child>
    <grandchild/>
    <grandchild/>
  </child>
</top>
},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:end_instruct, 'xml'],
                   [:start_element, :top],
                   [:start_element, :child],
                   [:start_element, :grandchild],
                   [:end_element, :grandchild],
                   [:end_element, :child],
                   [:start_element, :child],
                   [:start_element, :grandchild],
                   [:end_element, :grandchild],
                   [:start_element, :grandchild],
                   [:end_element, :grandchild],
                   [:end_element, :child],
                   [:end_element, :top],
                  ])
  end

  def test_sax_element_no_term
    Ox::default_options = $ox_sax_options
    parse_compare(%{
<top>
  <child/>
},
                  [[:start_element, :top],
                   [:start_element, :child],
                   [:end_element, :child],
                   [:error, "Start End Mismatch: element 'top' not closed", 4, 0],
                   [:end_element, :top],
                  ])
  end

  def test_sax_text
    Ox::default_options = $ox_sax_options
    parse_compare(%{<top>This is some text.</top>},
                  [[:start_element, :top],
                   [:text, "This is some text."],
                   [:end_element, :top]
                  ])
  end

  def test_sax_open_close_element
    Ox::default_options = $ox_sax_options
    parse_compare(%{<top><mid></mid></top>},
                  [[:start_element, :top],
                   [:start_element, :mid],
                   [:text, ""],
                   [:end_element, :mid],
                   [:end_element, :top]
                  ])
  end

  def test_sax_empty_element
    Ox::default_options = $ox_sax_options
    parse_compare(%{  <top>  </top>},
                  [[:start_element, :top],
                   [:text, " "],
                   [:end_element, :top]
                  ], AllSax, { :skip => :skip_white })
  end

  def test_sax_empty_element_name
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml version="1.0"?>
<top>
  <>
  <abc x="1">zzz</abc>
</top>},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:end_instruct, "xml"],
                   [:start_element, :top],
                   [:error, "Invalid Format: empty element", 3, 3],
                   [:start_element, :abc],
                   [:attr, :x, "1"],
                   [:text, "zzz"],
                   [:end_element, :abc],
                   [:end_element, :top],
                  ])
  end

  def test_sax_empty_element_noskip
    Ox::default_options = $ox_sax_options
    parse_compare(%{  <top>  </top>},
                  [[:text, "  "],
                   [:start_element, :top],
                   [:text, "  "],
                   [:end_element, :top]
                  ], AllSax, { :skip => :skip_none })
  end

  def test_sax_special
    Ox::default_options = $ox_sax_options
    parse_compare(%{<top name="A&amp;Z">This is &lt;some&gt; &pi; text.</top>},
                  [[:start_element, :top],
                   [:attr, :name, 'A&Z'],
                   [:text, "This is <some> \u03c0 text."],
                   [:end_element, :top]
                  ], AllSax, :convert_special => true)
  end

  def test_sax_bad_special
    Ox::default_options = $ox_sax_options
    parse_compare(%{<top name="&example;">&example;</top>},
                  [[:start_element, :top],
                   [:error, "Invalid Format: Invalid special character sequence", 1, 11],
                   [:attr, :name, '&example;'],
                   [:error, "Invalid Format: Invalid special character sequence", 1, 22],
                   [:text, "&example;"],
                   [:end_element, :top]
                  ], AllSax, :convert_special => true)
  end

  def test_sax_ignore_special
    Ox::default_options = $ox_sax_options
    parse_compare(%{<top name="&example;">&example;</top>},
                  [[:start_element, :top],
                   [:attr, :name, '&example;'],
                   [:text, "&example;"],
                   [:end_element, :top]
                  ], AllSax, :convert_special => false)
  end

  def test_sax_not_special
    Ox::default_options = $ox_sax_options
    parse_compare(%{<top name="A&amp;Z">This is &lt;some&gt; text.</top>},
                  [[:start_element, :top],
                   [:attr, :name, 'A&amp;Z'],
                   [:text, "This is &lt;some&gt; text."],
                   [:end_element, :top]
                  ], AllSax, :convert_special => false)
  end

  def test_sax_special_default
    Ox::default_options = $ox_sax_options
    orig = Ox::default_options[:convert_special]
    Ox::default_options = { :convert_special => true }
    parse_compare(%{<top name="A&amp;Z">This is &lt;some&gt; text.</top>},
                  [[:start_element, :top],
                   [:attr, :name, 'A&Z'],
                   [:text, "This is <some> text."],
                   [:end_element, :top]
                  ], AllSax)
    Ox::default_options = { :convert_special => orig }
  end

  def test_sax_not_special_default
    Ox::default_options = $ox_sax_options
    orig = Ox::default_options[:convert_special]
    Ox::default_options = { :convert_special => false }
    parse_compare(%{<top name="A&amp;Z">This is &lt;some&gt; text.</top>},
                  [[:start_element, :top],
                   [:attr, :name, 'A&amp;Z'],
                   [:text, "This is &lt;some&gt; text."],
                   [:end_element, :top]
                  ], AllSax)
    Ox::default_options = { :convert_special => orig }
  end

  def test_sax_whitespace
    Ox::default_options = $ox_sax_options
    parse_compare(%{<top> This is some text.</top>},
                  [[:start_element, :top],
                   [:text, " This is some text."],
                   [:end_element, :top]
                  ])
  end

  def test_sax_text_no_term
    Ox::default_options = $ox_sax_options
    parse_compare(%{<top>This is some text.},
                  [[:start_element, :top],
                   [:error, "Not Terminated: text not terminated", 1, 23],
                   [:text, "This is some text."],
                   [:error, "Start End Mismatch: element 'top' not closed", 1, 23],
                   [:end_element, :top]])
  end
  # TBD invalid chacters in text

  def test_sax_doctype
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml version="1.0"?>
<!DOCTYPE top PUBLIC "top.dtd">
<top/>
},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:end_instruct, 'xml'],
                   [:doctype, ' top PUBLIC "top.dtd"'],
                   [:start_element, :top],
                   [:end_element, :top]])
  end

  def test_sax_doctype_big
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml version="1.0"?>
<!DOCTYPE Objects [<!ELEMENT Objects (RentObjects)><!ENTITY euml "&#235;"><!ENTITY Atilde "&#195;">]>
<top/>
},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:end_instruct, 'xml'],
                   [:doctype, %{ Objects [<!ELEMENT Objects (RentObjects)><!ENTITY euml "&#235;"><!ENTITY Atilde "&#195;">]}],
                   [:start_element, :top],
                   [:end_element, :top]])
  end

  def test_sax_doctype_bad_order
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml version="1.0"?>
<top/>
<!DOCTYPE top PUBLIC "top.dtd">
},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:end_instruct, 'xml'],
                   [:start_element, :top],
                   [:end_element, :top],
                   [:error, "Out of Order: DOCTYPE can not come after an element", 3, 10],
                   [:doctype, ' top PUBLIC "top.dtd"']])
  end

  def test_sax_doctype_space
    Ox::default_options = $ox_sax_options
    parse_compare(%{
<! DOCTYPE top PUBLIC "top.dtd">
<top/>
},
                  [[:error, "Unexpected Character: <!DOCTYPE can not included spaces", 2, 4],
                   [:doctype, ' top PUBLIC "top.dtd"'],
                   [:start_element, :top],
                   [:end_element, :top]])
  end

  def test_sax_bad_bang
    Ox::default_options = $ox_sax_options
    parse_compare(%{<top>
  <! Bang Bang>
</top>
},
                  [[:start_element, :top],
                   [:error, "Unexpected Character: DOCTYPE, CDATA, or comment expected", 2, 6],
                   [:end_element, :top]])
  end

  def test_sax_comment_simple
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml version="1.0"?>
<!--First comment.-->
},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:end_instruct, 'xml'],
                   [:comment, 'First comment.']])
  end

  def test_sax_comment
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml version="1.0"?>
<!--First comment.-->
<top>Before<!--Nested comment.-->After</top>
},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:end_instruct, 'xml'],
                   [:comment, 'First comment.'],
                   [:start_element, :top],
                   [:text, 'Before'],
                   [:comment, 'Nested comment.'],
                   [:text, 'After'],
                   [:end_element, :top]])
  end

  def test_sax_comment_no_term
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml version="1.0"?>
<!--First comment.--
<top/>
},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:end_instruct, 'xml'],
                   [:error, "Not Terminated: comment not terminated", 3, 0],
                   [:comment, 'First comment.--'],
                   [:start_element, :top],
                   [:end_element, :top]])
  end

  def test_sax_cdata
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml version="1.0"?>
<top>
  <![CDATA[This is CDATA.]]>
</top>
},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:end_instruct, 'xml'],
                   [:start_element, :top],
                   [:cdata, 'This is CDATA.'],
                   [:end_element, :top]])
  end

  def test_sax_cdata_no_term
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml version="1.0"?>
<top>
  <![CDATA[This is CDATA.]]
</top>
},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:end_instruct, 'xml'],
                   [:start_element, :top],
                   [:error, "Not Terminated: CDATA not terminated", 4, 0],
                   [:cdata, "This is CDATA.]]"],
                   [:end_element, :top]])
  end

  def test_sax_cdata_empty
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml version="1.0"?>
<top>
  <child><![CDATA[]]></child>
  <child><![CDATA[This is CDATA.]]></child>
</top>
},
                  [[:instruct, 'xml'],
                   [:attr, :version, '1.0'],
                   [:end_instruct, 'xml'],
                   [:start_element, :top],
                   [:start_element, :child],
                   [:cdata, ''],
                   [:end_element, :child],
                   [:start_element, :child],
                   [:cdata, 'This is CDATA.'],
                   [:end_element, :child],
                   [:end_element, :top]])
  end

  def test_sax_mixed
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml version="1.0"?>
<?ox version="1.0" mode="object" circular="no" xsd_date="no"?>
<!DOCTYPE table PUBLIC "-//ox//DTD TABLE 1.0//EN" "http://www.ohler.com/DTDs/TestTable-1.0.dtd">
<table>
  <row id="00004">
    <cell id="A" type="Fixnum">1234</cell>
    <cell id="B" type="String">A string.</cell>
    <cell id="C" type="String">This is a longer string that stretches over a larger number of characters.</cell>
    <cell id="D" type="Float">-12.345</cell>
    <cell id="E" type="Date">2011-09-18 23:07:26 +0900</cell>
    <cell id="F" type="Image"><![CDATA[xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00]]></cell>
  </row>
</table>
},
                  [[:instruct, "xml"],
                   [:attr, :version, "1.0"],
                   [:end_instruct, 'xml'],
                   [:instruct, "ox"],
                   [:attr, :version, "1.0"],
                   [:attr, :mode, "object"],
                   [:attr, :circular, "no"],
                   [:attr, :xsd_date, "no"],
                   [:end_instruct, 'ox'],
                   [:doctype, " table PUBLIC \"-//ox//DTD TABLE 1.0//EN\" \"http://www.ohler.com/DTDs/TestTable-1.0.dtd\""],
                   [:start_element, :table],
                   [:start_element, :row],
                   [:attr, :id, "00004"],
                   [:start_element, :cell],
                   [:attr, :id, "A"],
                   [:attr, :type, "Fixnum"],
                   [:text, "1234"],
                   [:end_element, :cell],
                   [:start_element, :cell],
                   [:attr, :id, "B"],
                   [:attr, :type, "String"],
                   [:text, "A string."],
                   [:end_element, :cell],
                   [:start_element, :cell],
                   [:attr, :id, "C"],
                   [:attr, :type, "String"],
                   [:text, "This is a longer string that stretches over a larger number of characters."],
                   [:end_element, :cell],
                   [:start_element, :cell],
                   [:attr, :id, "D"],
                   [:attr, :type, "Float"],
                   [:text, "-12.345"],
                   [:end_element, :cell],
                   [:start_element, :cell],
                   [:attr, :id, "E"],
                   [:attr, :type, "Date"],
                   [:text, "2011-09-18 23:07:26 +0900"],
                   [:end_element, :cell],
                   [:start_element, :cell],
                   [:attr, :id, "F"],
                   [:attr, :type, "Image"],
                   [:cdata, "xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00"],
                   [:end_element, :cell],
                   [:end_element, :row],
                   [:end_element, :table]])
  end

  def test_sax_encoding
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml version="1.0" encoding="UTF-8"?>
<top>ピーター</top>
},
                  [[:instruct, "xml"],
                   [:attr, :version, "1.0"],
                   [:attr, :encoding, "UTF-8"],
                   [:end_instruct, 'xml'],
                   [:start_element, :top],
                   [:text, 'ピーター'],
                   [:end_element, :top]])
  end

  def test_sax_bom
    Ox::default_options = $ox_sax_options
    xml = %{\xEF\xBB\xBF<?xml?>
<top>ピーター</top>
}
    if !RUBY_VERSION.start_with?('1.8')
      xml.force_encoding('ASCII')
    end
    parse_compare(xml,
                  [[:instruct, "xml"],
                   [:end_instruct, 'xml'],
                   [:start_element, :top],
                   [:text, 'ピーター'],
                   [:end_element, :top]])
  end

  def test_sax_full_encoding
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml version="1.0" encoding="UTF-8"?>
<いち name="ピーター" つま="まきえ">ピーター is katakana</いち>
},
                  [[:instruct, "xml"],
                   [:attr, :version, "1.0"],
                   [:attr, :encoding, "UTF-8"],
                   [:end_instruct, 'xml'],
                   [:start_element, 'いち'.to_sym],
                   [:attr, :name, 'ピーター'],
                   [:attr, 'つま'.to_sym, 'まきえ'],
                   [:text, 'ピーター is katakana'],
                   [:end_element, 'いち'.to_sym]])
  end

  def test_sax_amp_hash
    Ox::default_options = $ox_sax_options
    parse_compare(%{<?xml version="1.0" encoding="UTF-8"?>
<text>&#233; is e with an accent</text>
},
                  [[:instruct, "xml"],
                   [:attr, :version, "1.0"],
                   [:attr, :encoding, "UTF-8"],
                   [:end_instruct, 'xml'],
                   [:start_element, :text],
                   [:text, 'é is e with an accent'],
                   [:end_element, :text]], AllSax, :convert_special => true)
  end

  def test_sax_implied_encoding
    Ox::default_options = $ox_sax_options
    if RUBY_VERSION.start_with?('1.8') || RUBY_DESCRIPTION.start_with?('rubinius 2.0.0dev')
      assert(true)
    else
      xml = %{<?xml version="1.0"?>
<top>ピーター</top>
}
      xml.force_encoding('UTF-8')
      handler = AllSax.new()
      input = StringIO.new(xml)
      Ox.sax_parse(handler, input)
      assert_equal('UTF-8', handler.calls.assoc(:text)[1].encoding.to_s)
      # now try a different one
      xml.force_encoding('US-ASCII')
      handler = AllSax.new()
      input = StringIO.new(xml)
      Ox.sax_parse(handler, input)
      assert_equal('US-ASCII', handler.calls.assoc(:text)[1].encoding.to_s)
    end
  end

  def test_sax_non_utf8_encoding
    Ox::default_options = $ox_sax_options
    if RUBY_VERSION.start_with?('1.8') || RUBY_DESCRIPTION.start_with?('rubinius 2.0.0dev')
      assert(true)
    else
      xml = %{<?xml version="1.0" encoding="Windows-1251"?>
<top>тест</top>
}
      handler = AllSax.new()
      input = StringIO.new(xml)
      Ox.sax_parse(handler, input)

      content = handler.calls.assoc(:text)[1]
      assert_equal('Windows-1251', content.encoding.to_s)
      assert_equal('тест'.force_encoding('Windows-1251'), content)
    end
  end

  def test_sax_value_fixnum
    Ox::default_options = $ox_sax_options
    handler = TypeSax.new(:as_i)
    Ox.sax_parse(handler, StringIO.new(%{<top>7</top>}))
    assert_equal(7, handler.item)
  end

  def test_sax_value_string
    Ox::default_options = $ox_sax_options
    handler = TypeSax.new(:as_s)
    Ox.sax_parse(handler, StringIO.new(%{<top>cheese</top>}))
    assert_equal('cheese', handler.item)
  end

  def test_sax_skip_none
    Ox::default_options = $ox_sax_options
    Ox::default_options = { :skip => :skip_none }

    handler = TypeSax.new(:as_s)
    xml = %{<top>  Pete\r\n  Ohler\r</top>}
    Ox.sax_parse(handler, StringIO.new(xml))
    assert_equal(%{  Pete\r\n  Ohler\r}, handler.item)
  end

  def test_sax_skip_none_nested
    Ox::default_options = $ox_sax_options
    Ox::default_options = { :skip => :skip_none }

    handler = AllSax.new()
    xml = %{<top>  <child>Pete\r</child>\n <child> Ohler\r</child></top>}
    Ox.sax_parse(handler, StringIO.new(xml))
    assert_equal([[:start_element, :top],
                  [:text, "  "],
                  [:start_element, :child],
                  [:text, "Pete\r"],
                  [:end_element, :child],
                  [:text, "\n "],
                  [:start_element, :child],
                  [:text, " Ohler\r"],
                  [:end_element, :child],
                  [:end_element, :top]], handler.calls)
  end

  def test_sax_skip_return
    Ox::default_options = $ox_sax_options
    handler = TypeSax.new(:as_s)
    xml = %{<top>  Pete\r\n  Ohler\r</top>}
    Ox.sax_parse(handler, StringIO.new(xml), :skip => :skip_return)
    assert_equal(%{  Pete\n  Ohler\r}, handler.item)
  end

  def test_sax_skip_white
    Ox::default_options = $ox_sax_options
    handler = TypeSax.new(:as_s)
    xml = %{<top>  Pete\r\n  Ohler\r</top>}
    Ox.sax_parse(handler, StringIO.new(xml), :skip => :skip_white)
    assert_equal(%{ Pete Ohler }, handler.item)
  end

  def test_sax_value_float
    Ox::default_options = $ox_sax_options
    handler = TypeSax.new(:as_f)
    Ox.sax_parse(handler, StringIO.new(%{<top>7</top>}))
    assert_equal(7.0, handler.item)
  end

  def test_sax_value_sym
    Ox::default_options = $ox_sax_options
    handler = TypeSax.new(:as_sym)
    Ox.sax_parse(handler, StringIO.new(%{<top>cheese</top>}))
    assert_equal(:cheese, handler.item)
  end

  def test_sax_value_bool
    Ox::default_options = $ox_sax_options
    handler = TypeSax.new(:as_bool)
    Ox.sax_parse(handler, StringIO.new(%{<top>true</top>}))
    assert_equal(true, handler.item)
  end

  def test_sax_value_nil
    Ox::default_options = $ox_sax_options
    handler = TypeSax.new(:as_i)
    Ox.sax_parse(handler, StringIO.new(%{<top></top>}))
    assert_equal(nil, handler.item)
  end

  def test_sax_value_time
    Ox::default_options = $ox_sax_options
    t = Time.local(2012, 1, 5, 10, 20, 30)
    handler = TypeSax.new(:as_time)
    Ox.sax_parse(handler, StringIO.new(%{<top>#{t}</top>}))
    assert_equal(t.sec, handler.item.sec)
    assert_equal(t.usec, handler.item.usec)
    t = Time.now
    Ox.sax_parse(handler, StringIO.new(%{<top>%d.%06d</top>} % [t.sec, t.usec]))
    assert_equal(t.sec, handler.item.sec)
    assert_equal(t.usec, handler.item.usec)
  end

  def test_sax_attr_value_fixnum
    Ox::default_options = $ox_sax_options
    handler = TypeSax.new(nil)
    Ox.sax_parse(handler, StringIO.new(%{<top as_i="7"/>}))
    assert_equal(7, handler.item)
  end

  def test_sax_attr_value_string
    Ox::default_options = $ox_sax_options
    handler = TypeSax.new(nil)
    Ox.sax_parse(handler, StringIO.new(%{<top as_s="cheese"/>}))
    assert_equal('cheese', handler.item)
  end

  def test_sax_attr_value_float
    Ox::default_options = $ox_sax_options
    handler = TypeSax.new(nil)
    Ox.sax_parse(handler, StringIO.new(%{<top as_f="7"/>}))
    assert_equal(7.0, handler.item)
  end

  def test_sax_attr_value_sym
    Ox::default_options = $ox_sax_options
    handler = TypeSax.new(nil)
    Ox.sax_parse(handler, StringIO.new(%{<top as_sym="cheese"/>}))
    assert_equal(:cheese, handler.item)
  end

  def test_sax_attr_value_bool
    Ox::default_options = $ox_sax_options
    handler = TypeSax.new(nil)
    Ox.sax_parse(handler, StringIO.new(%{<top as_bool="true"/>}))
    assert_equal(true, handler.item)
  end

  def test_sax_attr_value_nil
    Ox::default_options = $ox_sax_options
    handler = TypeSax.new(nil)
    Ox.sax_parse(handler, StringIO.new(%{<top as_i=""/>}))
    assert_equal(nil, handler.item)
  end

  def test_sax_attr_value_time
    Ox::default_options = $ox_sax_options
    t = Time.local(2012, 1, 5, 10, 20, 30)
    handler = TypeSax.new(nil)
    Ox.sax_parse(handler, StringIO.new(%{<top as_time="#{t}"/>}))
    assert_equal(t.sec, handler.item.sec)
    assert_equal(t.usec, handler.item.usec)
    t = Time.local(2012, 1, 5, 10, 20, 30.123456)
    Ox.sax_parse(handler, StringIO.new(%{<top as_time="%d.%06d"/>} % [t.sec, t.usec]))
    assert_equal(t.sec, handler.item.sec)
    assert_equal(t.usec, handler.item.usec)
  end

  def test_sax_nested_same_prefix
    Ox::default_options = $ox_sax_options
    handler = ErrorSax.new
    Ox.sax_parse(handler, StringIO.new(%{<?xml version="1.0"?><abc><abcd/></abc>}))
    assert_equal([], handler.errors)
    Ox.sax_parse(handler, StringIO.new(%{<?xml version="1.0"?><abc><ab/></abc>}))
    assert_equal([], handler.errors)
    Ox.sax_parse(handler, StringIO.new(%{<?xml version="1.0"?><abcd><abcd/></abcd>}))
    assert_equal([], handler.errors)
    Ox.sax_parse(handler, StringIO.new(%{<?xml version="1.0"?><ab><a/></ab>}))
    assert_equal([], handler.errors)
    Ox.sax_parse(handler, StringIO.new(%{<?xml version="1.0"?><abcdefghijklmnop></abcdefghijklmnop>}))
    assert_equal([], handler.errors)
  end

  def test_sax_offset_start
    Ox::default_options = $ox_sax_options
    handler = AllSax.new()
    xml = StringIO.new(%|
this is not part of the xml document
<top>this is xml</top>
|)
    xml.seek(37)
    Ox.sax_parse(handler, xml)
    assert_equal([[:start_element, :top],
                  [:text, "this is xml"],
                  [:end_element, :top]], handler.calls)
  end

  def test_sax_tolerant
    Ox::default_options = $ox_sax_options
    xml = %{<!doctype HTML>
<html lang=en>
  <head garbage='trash'>
    <bad attr="some&#xthing">
    <bad alone>
  </head>
  <body>
  This is a test of the &tolerant&# effort option.
  </body>
</html>
<p>after thought</p>
}
    parse_compare(xml,
                  [
                   [:doctype, " HTML"],
                   [:start_element, :html],
                   [:error, "Unexpected Character: attribute value not in quotes", 2, 12],
                   [:attr, :lang, "en"],
                   [:start_element, :head],
                   [:attr, :garbage, "trash"],
                   [:error, "Invalid Element: bad is not a valid element type for a HTML document type.", 4, 9],
                   [:start_element, :bad],
                   [:attr, :attr, "some&#xthing"],
                   [:error, "Invalid Element: bad is not a valid element type for a HTML document type.", 5, 9],
                   [:start_element, :bad],
                   [:error, "Unexpected Character: no attribute value", 5, 15],
                   [:attr, :alone, ""],
                   [:text, "\n  "],
                   [:error, "Start End Mismatch: element 'head' close does not match 'bad' open", 6, 3],
                   [:end_element, :bad],
                   [:end_element, :bad],
                   [:end_element, :head],
                   [:start_element, :body],
                   [:text, "\n  This is a test of the &tolerant&# effort option.\n  "],
                   [:end_element, :body],
                   [:end_element, :html],
                   [:error, "Out of Order: multiple top level elements", 11, 1],
                   [:start_element, :p],
                   [:text, "after thought"],
                   [:end_element, :p]],
                  AllSax, :convert_special => false, :smart => true)
  end

  def test_sax_not_symbolized
    Ox::default_options = $ox_sax_options
    xml = %{<!DOCTYPE HTML>
<html lang="en">
  <head>
  </head>
  <body>
  This is a some text.
  </body>
</html>
}
    parse_compare(xml,
                  [
                   [:doctype, " HTML"],
                   [:start_element, "html"],
                   [:attr, "lang", "en"],
                   [:start_element, "head"],
                   [:text, "\n  "],
                   [:end_element, "head"],
                   [:start_element, "body"],
                   [:text, "\n  This is a some text.\n  "],
                   [:end_element, "body"],
                   [:end_element, "html"]
                  ],
                  AllSax, :symbolize => false, :smart => true)
  end

  def test_sax_script
    Ox::default_options = $ox_sax_options
    html = %|<!DOCTYPE HTML>
<html lang="en">
  <head>
  </head>
  <body>
    <script type='text/javascript'>
      def fake(c,d) {
        x = "</script";
        return c<d;
      }
    </ script >
  </body>
</html>
|
    parse_compare(html,
                  [[:doctype, " HTML"],
                   [:start_element, :html],
                   [:attr, :lang, "en"],
                   [:start_element, :head],
                   [:text, "\n  "],
                   [:end_element, :head],
                   [:start_element, :body],
                   [:start_element, :script],
                   [:attr, :type, "text/javascript"],
                   [:text, "\n      def fake(c,d) {\n        x = \"</script\";\n        return c<d;\n      }\n    "],
                   [:end_element, :script],
                   [:end_element, :body],
                   [:end_element, :html]
                  ],
                  AllSax, :smart => true)
  end

  def test_sax_namespace_no_strip
    Ox::default_options = $ox_sax_options
    handler = AllSax.new()
    Ox.sax_parse(handler, '<spaced:one><two spaced:out="no" other:space="yes">inside</two></spaced:one>')
    assert_equal([[:start_element, :"spaced:one"],
                  [:start_element, :two],
                  [:attr, :"spaced:out", "no"],
                  [:attr, :"other:space", "yes"],
                  [:text, "inside"],
                  [:end_element, :two],
                  [:end_element, :"spaced:one"]], handler.calls)
  end

  def test_sax_namespace_strip_wild
    Ox::default_options = $ox_sax_options
    handler = AllSax.new()
    Ox.sax_parse(handler,
                 '<spaced:one><two spaced:out="no" other:space="yes">inside</two></spaced:one>',
                 :strip_namespace => true)
    assert_equal([[:start_element, :one],
                  [:start_element, :two],
                  [:attr, :"out", "no"],
                  [:attr, :space, "yes"],
                  [:text, "inside"],
                  [:end_element, :two],
                  [:end_element, :one]], handler.calls)
  end

  def test_sax_namespace_strip
    Ox::default_options = $ox_sax_options
    Ox::default_options = { :strip_namespace => "spaced" }

    handler = AllSax.new()
    Ox.sax_parse(handler, '<spaced:one><two spaced:out="no" other:space="yes">inside</two></spaced:one>')
    assert_equal([[:start_element, :one],
                  [:start_element, :two],
                  [:attr, :out, "no"],
                  [:attr, :"other:space", "yes"],
                  [:text, "inside"],
                  [:end_element, :two],
                  [:end_element, :one]], handler.calls)
  end

  def test_sax_html_inactive
    Ox::default_options = $ox_sax_options
    handler = AllSax.new()

    Ox.sax_html(handler, '<html><h1>title</h1><hr/><p>Hello</p></html>', :overlay => {'hr'=>:inactive})
    assert_equal([[:start_element, :html],
                  [:start_element, :h1],
                  [:text, "title"],
                  [:end_element, :h1],
                  [:start_element, :p],
                  [:text, "Hello"],
                  [:end_element, :p],
                  [:end_element, :html]], handler.calls)
  end

  def test_sax_html_overlay_mod
    Ox::default_options = $ox_sax_options
    handler = AllSax.new()
    overlay = Ox.sax_html_overlay()
    overlay['hr'] = :inactive
    Ox.sax_html(handler, '<html><h1>title</h1><hr/><p>Hello</p></html>', :overlay => overlay)
    assert_equal([[:start_element, :html],
                  [:start_element, :h1],
                  [:text, "title"],
                  [:end_element, :h1],
                  [:start_element, :p],
                  [:text, "Hello"],
                  [:end_element, :p],
                  [:end_element, :html]], handler.calls)
  end

  def test_sax_html_active
    Ox::default_options = $ox_sax_options
    handler = AllSax.new()
    overlay = Ox.sax_html_overlay()
    overlay.each_key { |k| overlay[k] = :inactive }
    overlay['h1'] = :active
    overlay['p'] = :active

    Ox.sax_html(handler, '<html class="fuzzy"><h1>title</h1><hr/><p>Hello</p></html>', :overlay => overlay)
    assert_equal([[:start_element, :h1],
                  [:text, "title"],
                  [:end_element, :h1],
                  [:start_element, :p],
                  [:text, "Hello"],
                  [:end_element, :p]], handler.calls)
  end

  def test_sax_html_default_overlay_mod
    Ox::default_options = $ox_sax_options
    handler = AllSax.new()
    overlay = Ox.sax_html_overlay()
    overlay['hr'] = :inactive
    Ox::default_options = { :overlay => overlay }
    Ox.sax_html(handler, '<html><h1>title</h1><hr/><p>Hello</p></html>')
    assert_equal([[:start_element, :html],
                  [:start_element, :h1],
                  [:text, "title"],
                  [:end_element, :h1],
                  [:start_element, :p],
                  [:text, "Hello"],
                  [:end_element, :p],
                  [:end_element, :html]], handler.calls)
  end

  def test_sax_html_block
    Ox::default_options = $ox_sax_options
    handler = AllSax.new()
    overlay = Ox.sax_html_overlay()
    overlay['table'] = :block

    Ox.sax_html(handler, '<html><h1>title</h1><table><!--comment--><tr><td>one</td></tr></table></html>', :overlay => overlay)
    assert_equal([[:start_element, :html],
                  [:start_element, :h1],
                  [:text, "title"],
                  [:end_element, :h1],
                  [:end_element, :html]], handler.calls)
  end

  def test_sax_html_off
    Ox::default_options = $ox_sax_options
    handler = AllSax.new()
    overlay = Ox.sax_html_overlay()
    overlay['table'] = :off
    overlay['td'] = :off
    overlay['!--'] = :off

    Ox.sax_html(handler, '<html><h1>title</h1><table><!--comment--><hr/><tr><td>one</td></tr></table></html>', :overlay => overlay)
    assert_equal([[:start_element, :html],
                  [:start_element, :h1],
                  [:text, "title"],
                  [:end_element, :h1],
                  [:start_element, :hr],
                  [:end_element, :hr],
                  [:start_element, :tr],
                  [:end_element, :tr],
                  [:end_element, :html]], handler.calls)
  end

  def test_sax_html_inactive_full_map
    Ox::default_options = $ox_sax_options
    handler = AllSax.new()
    overlay = Ox.sax_html_overlay()
    overlay['table'] = :inactive
    overlay['td'] = :inactive

    Ox.sax_html(handler, '<html><h1>title</h1><table><!--comment--><tr><td>one</td></tr></table></html>', :overlay => overlay)
    assert_equal([[:start_element, :html],
                  [:start_element, :h1],
                  [:text, "title"],
                  [:end_element, :h1],
                  [:comment, "comment"],
                  [:start_element, :tr],
                  [:text, "one"],
                  [:end_element, :tr],
                  [:end_element, :html]], handler.calls)
  end


  def test_sax_html_abort
    Ox::default_options = $ox_sax_options
    handler = AllSax.new()
    overlay = Ox.sax_html_overlay()
    overlay['table'] = :abort

    Ox.sax_html(handler, '<html><h1>title</h1><table><!--comment--><tr><td>one</td></tr></table></html>', :overlay => overlay)
    assert_equal([[:start_element, :html],
                  [:start_element, :h1],
                  [:text, "title"],
                  [:end_element, :h1],
                  [:abort, :table],
                 ], handler.calls)
  end

end
