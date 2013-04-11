#!/usr/bin/env ruby
# encoding: UTF-8

# Ubuntu does not accept arguments to ruby when called using env. To get warnings to show up the -w options is
# required. That can be set in the RUBYOPT environment variable.
# export RUBYOPT=-w

$VERBOSE = true

$: << File.join(File.dirname(__FILE__), "../../lib")
$: << File.join(File.dirname(__FILE__), "../../ext")
$: << File.join(File.dirname(__FILE__), ".")

require 'stringio'
require 'test/unit'
require 'optparse'

require 'ox'

require 'helpers'
require 'smart_test'

opts = OptionParser.new
opts.on("-h", "--help", "Show this display")                { puts opts; Process.exit!(0) }
opts.parse(ARGV)

class SaxBaseTest < ::Test::Unit::TestCase
  include SaxTestHelpers

  def test_sax_io_pipe
    handler = AllSax.new()
    input,w = IO.pipe
    w << %{<top/>}
    w.close
    Ox.sax_parse(handler, input)
    assert_equal([[:start_element, :top],
                  [:end_element, :top]], handler.calls)
  end

  def test_sax_io_file
    handler = AllSax.new()
    input = IO.open(IO.sysopen(File.join(File.dirname(__FILE__), 'basic.xml')))
    Ox.sax_parse(handler, input)
    assert_equal([[:start_element, :top],
                  [:end_element, :top]], handler.calls)
  end

  def test_sax_instruct_simple
    parse_compare(%{<?xml?>}, [[:instruct, 'xml'],
                               [:end_instruct, 'xml']])
  end

  def test_sax_instruct_blank
    parse_compare(%{<?xml?>}, [], StartSax)
  end

  def test_sax_instruct_attrs
    parse_compare(%{<?xml version="1.0" encoding="UTF-8"?>},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:attr, :encoding, "UTF-8"],
                   [:end_instruct, 'xml']])
  end

  def test_sax_instruct_noattrs
    parse_compare(%{<?bad something?>},
                  [[:instruct, 'bad'],
                   [:text, "something"],
                   [:end_instruct, 'bad']])
  end

  def test_sax_instruct_loose
    parse_compare(%{<? xml
version = "1.0"
encoding = "UTF-8" ?>},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:attr, :encoding, "UTF-8"],
                   [:end_instruct, 'xml']])
  end

  def test_sax_instruct_pi
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
    parse_compare(%{<?xml?
<top/>},
                  [[:instruct, "xml"],
                   [:error, "Not Terminated: instruction not terminated", 1, 6],
                   [:end_instruct, "xml"],
                   [:start_element, :top],
                   [:end_element, :top]])
  end

  def test_sax_element_simple
    parse_compare(%{<top/>},
                  [[:start_element, :top],
                   [:end_element, :top]])
  end

  def test_sax_element_attrs
    parse_compare(%{<top x="57" y='42' z=33/>},
                  [[:start_element, :top],
                   [:attr, :x, "57"],
                   [:attr, :y, "42"],
                   [:error, "Unexpected Character: attribute value not in quotes", 1, 22],
                   [:attr, :z, "33"],
                   [:end_element, :top]])
  end
  def test_sax_element_start_end
    parse_compare(%{<top></top>},
                  [[:start_element, :top],
                   [:end_element, :top]])
  end

  def test_sax_two_top
    parse_compare(%{<top/><top/>},
                  [[:start_element, :top],
                   [:end_element, :top],
                   [:error, "Out of Order: multiple top level elements", 1, 7],
                   [:start_element, :top],
                   [:end_element, :top]])
  end

  def test_sax_nested_elements
    parse_compare(%{<top>
  <child>
    <grandchild/>
  </child>
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

  def test_sax_nested1
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
    parse_compare(%{<?xml version="1.0"?>
<!DOCTYPE top PUBLIC "top.dtd">
<top attr="one">
  <!-- family -->
  <child>
    <grandchild/>
    <![CDATA[see data.]]>
  </child>Some text.
</top>},
                  [[:instruct, 'xml', 1, 1],
                   [:attr, :version, "1.0", 1, 6],
                   [:end_instruct, 'xml', 1, 20],
                   [:doctype, " top PUBLIC \"top.dtd\"", 2, 1],
                   [:start_element, :top, 3, 1],
                   [:attr, :attr, "one", 3, 6],
                   [:comment, " family ", 4, 4],
                   [:start_element, :child, 5, 3],
                   [:start_element, :grandchild, 6, 5],
                   [:end_element, :grandchild, 6, 17],
                   [:cdata, "see data.", 7, 5],
                   [:end_element, :child, 8, 3],
                   [:text, "Some text.\n", 8, 11],
                   [:end_element, :top, 9, 1],
                  ], LineColSax)
  end

  def test_sax_element_name_mismatch
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
    parse_compare(%{
<top>
  <child/>
},
                  [[:start_element, :top],
                   [:start_element, :child],
                   [:end_element, :child],
                   [:error, "Start End Mismatch: element 'top' not closed", 4, 1],
                   [:end_element, :top],
                  ])
  end

  def test_sax_text
    parse_compare(%{<top>This is some text.</top>},
                  [[:start_element, :top],
                   [:text, "This is some text."],
                   [:end_element, :top]
                  ])
  end

  def test_sax_open_close_element
    parse_compare(%{<top><mid></mid></top>},
                  [[:start_element, :top],
                   [:start_element, :mid],
                   [:end_element, :mid],
                   [:end_element, :top]
                  ])
  end

  def test_sax_empty_element
    parse_compare(%{<top></top>},
                  [[:start_element, :top],
                   [:end_element, :top]
                  ])
  end

  def test_sax_special
    parse_compare(%{<top name="A&amp;Z">This is &lt;some&gt; text.</top>},
                  [[:start_element, :top],
                   [:attr, :name, 'A&Z'],
                   [:text, "This is <some> text."],
                   [:end_element, :top]
                  ], AllSax, :convert_special => true)
  end

  def test_sax_whitespace
    parse_compare(%{<top> This is some text.</top>},
                  [[:start_element, :top],
                   [:text, " This is some text."],
                   [:end_element, :top]
                  ])
  end

  def test_sax_text_no_term
    parse_compare(%{<top>This is some text.},
                  [[:start_element, :top],
                   [:error, "Not Terminated: text not terminated", 1, 23],
                   [:text, "This is some text."],
                   [:error, "Start End Mismatch: element 'top' not closed", 1, 23],
                   [:end_element, :top]])
  end
  # TBD invalid chacters in text

  def test_sax_doctype
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

  def test_sax_doctype_bad_order
    parse_compare(%{<?xml version="1.0"?>
<top/>
<!DOCTYPE top PUBLIC "top.dtd">
},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:end_instruct, 'xml'],
                   [:start_element, :top],
                   [:end_element, :top],
                   [:error, "Out of Order: DOCTYPE can not come after an element", 3, 11],
                   [:doctype, ' top PUBLIC "top.dtd"']])
  end

  def test_sax_doctype_space
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
    parse_compare(%{<top>
  <! Bang Bang>
</top>
},
                  [[:start_element, :top],
                   [:error, "Unexpected Character: DOCTYPE, CDATA, or comment expected", 2, 6],
                   [:end_element, :top]])
  end

  def test_sax_comment_simple
    parse_compare(%{<?xml version="1.0"?>
<!--First comment.-->
},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:end_instruct, 'xml'],
                   [:comment, 'First comment.']])
  end

  def test_sax_comment
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
    parse_compare(%{<?xml version="1.0"?>
<!--First comment.--
<top/>
},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:end_instruct, 'xml'],
                   [:error, "Not Terminated: comment not terminated", 3, 1],
                   [:comment, 'First comment.--'],
                   [:start_element, :top],
                   [:end_element, :top]])
  end

  def test_sax_cdata
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
    parse_compare(%{<?xml version="1.0"?>
<top>
  <![CDATA[This is CDATA.]]
</top>
},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:end_instruct, 'xml'],
                   [:start_element, :top],
                   [:error, "Not Terminated: CDATA not terminated", 4, 1],
                   [:cdata, "This is CDATA.]]"],
                   [:end_element, :top]])
  end

  def test_sax_cdata_empty
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
    if RUBY_VERSION.start_with?('1.8')
      assert(true)
    else
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
  end

  def test_sax_bom
    if RUBY_VERSION.start_with?('1.8')
      assert(true)
    else
      xml = %{\xEF\xBB\xBF<?xml?>
<top>ピーター</top>
}
      xml.force_encoding('ASCII')
      parse_compare(xml,
                    [[:instruct, "xml"],
                     [:end_instruct, 'xml'],
                     [:start_element, :top],
                     [:text, 'ピーター'],
                     [:end_element, :top]])
    end
  end

  def test_sax_full_encoding
    if RUBY_VERSION.start_with?('1.8')
      assert(true)
    else
      parse_compare(%{<?xml version="1.0" encoding="UTF-8"?>
<いち name="ピーター" つま="まきえ">ピーター</いち>
},
                    [[:instruct, "xml"],
                     [:attr, :version, "1.0"],
                     [:attr, :encoding, "UTF-8"],
                     [:end_instruct, 'xml'],
                     [:start_element, 'いち'.to_sym],
                     [:attr, :name, 'ピーター'],
                     [:attr, 'つま'.to_sym, 'まきえ'],
                     [:text, 'ピーター'],
                     [:end_element, 'いち'.to_sym]])
    end
  end

  def test_sax_implied_encoding
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
    handler = TypeSax.new(:as_i)
    Ox.sax_parse(handler, StringIO.new(%{<top>7</top>}))
    assert_equal(7, handler.item)
  end

  def test_sax_value_string
    handler = TypeSax.new(:as_s)
    Ox.sax_parse(handler, StringIO.new(%{<top>cheese</top>}))
    assert_equal('cheese', handler.item)
  end

  def test_sax_value_float
    handler = TypeSax.new(:as_f)
    Ox.sax_parse(handler, StringIO.new(%{<top>7</top>}))
    assert_equal(7.0, handler.item)
  end

  def test_sax_value_sym
    handler = TypeSax.new(:as_sym)
    Ox.sax_parse(handler, StringIO.new(%{<top>cheese</top>}))
    assert_equal(:cheese, handler.item)
  end

  def test_sax_value_bool
    handler = TypeSax.new(:as_bool)
    Ox.sax_parse(handler, StringIO.new(%{<top>true</top>}))
    assert_equal(true, handler.item)
  end

  def test_sax_value_nil
    handler = TypeSax.new(:as_i)
    Ox.sax_parse(handler, StringIO.new(%{<top></top>}))
    assert_equal(nil, handler.item)
  end

  def test_sax_value_time
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
    handler = TypeSax.new(nil)
    Ox.sax_parse(handler, StringIO.new(%{<top as_i="7"/>}))
    assert_equal(7, handler.item)
  end

  def test_sax_attr_value_string
    handler = TypeSax.new(nil)
    Ox.sax_parse(handler, StringIO.new(%{<top as_s="cheese"/>}))
    assert_equal('cheese', handler.item)
  end

  def test_sax_attr_value_float
    handler = TypeSax.new(nil)
    Ox.sax_parse(handler, StringIO.new(%{<top as_f="7"/>}))
    assert_equal(7.0, handler.item)
  end

  def test_sax_attr_value_sym
    handler = TypeSax.new(nil)
    Ox.sax_parse(handler, StringIO.new(%{<top as_sym="cheese"/>}))
    assert_equal(:cheese, handler.item)
  end

  def test_sax_attr_value_bool
    handler = TypeSax.new(nil)
    Ox.sax_parse(handler, StringIO.new(%{<top as_bool="true"/>}))
    assert_equal(true, handler.item)
  end

  def test_sax_attr_value_nil
    handler = TypeSax.new(nil)
    Ox.sax_parse(handler, StringIO.new(%{<top as_i=""/>}))
    assert_equal(nil, handler.item)
  end

  def test_sax_attr_value_time
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

  def test_sax_tolerant
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
                   [:error, "Case Error: expected DOCTYPE all in caps", 1, 10],
                   [:doctype, " HTML"],
                   [:start_element, :html],
                   [:error, "Unexpected Character: attribute value not in quotes", 2, 13],
                   [:attr, :lang, "en"],
                   [:start_element, :head],
                   [:attr, :garbage, "trash"],
                   [:error, "Invalid Element: bad is not a valid element type for a HTML document type.", 4, 10],
                   [:start_element, :bad],
                   [:error, "Not Terminated: special character does not end with a semicolon", 4, 30],
                   [:attr, :attr, "some&#xthing"],
                   [:error, "Invalid Element: bad is not a valid element type for a HTML document type.", 5, 10],
                   [:start_element, :bad],
                   [:error, "Unexpected Character: no attribute value", 5, 16],
                   [:attr, :alone, ""],
                   [:error, "Start End Mismatch: element 'head' close does not match 'bad' open", 6, 3],
                   [:end_element, :bad],
                   [:end_element, :bad],
                   [:end_element, :head],
                   [:start_element, :body],
                   [:text, "\n  This is a test of the &tolerant&# effort option.\n  "],
                   [:end_element, :body],
                   [:end_element, :html],
                   [:error, "Out of Order: multiple top level elements", 12, 2],
                   [:start_element, :p],
                   [:text, "after thought"],
                   [:end_element, :p]],
                  AllSax, :convert_special => false, :smart => true)
  end

  def test_sax_not_symbolized
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
                   [:end_element, "head"],
                   [:start_element, "body"],
                   [:text, "\n  This is a some text.\n  "],
                   [:end_element, "body"],
                   [:end_element, "html"]
                  ],
                  AllSax, :symbolize => false, :smart => true)
  end
end
