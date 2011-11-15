#!/usr/bin/env ruby -wW1
# encoding: UTF-8

$: << File.join(File.dirname(__FILE__), "../lib")
$: << File.join(File.dirname(__FILE__), "../ext")

require 'stringio'
require 'test/unit'
require 'optparse'
require 'ox'

opts = OptionParser.new
opts.on("-h", "--help", "Show this display")                { puts opts; Process.exit!(0) }
files = opts.parse(ARGV)

class StartSax < ::Ox::Sax
  attr_accessor :calls

  def initialize()
    @calls = []
  end

  def start_element(name)
    @calls << [:start_element, name]
  end

  def attr(name, value)
    @calls << [:attr, name, value]
  end
end

class AllSax < StartSax
  def initialize()
    super
  end

  def instruct(target)
    @calls << [:instruct, target]
  end

  def doctype(value)
    @calls << [:doctype, value]
  end

  def comment(value)
    @calls << [:comment, value]
  end

  def cdata(value)
    @calls << [:cdata, value]
  end

  def text(value)
    @calls << [:text, value]
  end

  def end_element(name)
    @calls << [:end_element, name]
  end
  
  def error(message, line, column)
    @calls << [:error, message, line, column]
  end
end

class Func < ::Test::Unit::TestCase

  def test_sax_io_pipe
    handler = AllSax.new()
    input,w = IO.pipe
    w << %{<top/>}
    w.close
    Ox.sax_parse(handler, input)
    assert_equal(handler.calls,
                 [[:start_element, :top],
                  [:end_element, :top]])
  end

  def test_sax_io_file
    handler = AllSax.new()
    input = IO.open(IO.sysopen(File.join(File.dirname(__FILE__), 'basic.xml')))
    Ox.sax_parse(handler, input)
    assert_equal(handler.calls,
                 [[:start_element, :top],
                  [:end_element, :top]])
  end

  def parse_compare(xml, expected, handler_class=AllSax, special=false)
    handler = handler_class.new()
    input = StringIO.new(xml)
    if special
      Ox.sax_parse(handler, input, :convert_special => true)
    else
      Ox.sax_parse(handler, input)
    end
    assert_equal(expected, handler.calls)
  end
  
  def test_sax_instruct_simple
    parse_compare(%{<?xml?>}, [[:instruct, 'xml']])
  end

  def test_sax_instruct_blank
    parse_compare(%{<?xml?>}, [], StartSax)
  end

  def test_sax_instruct_attrs
    parse_compare(%{<?xml version="1.0" encoding="UTF-8"?>},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:attr, :encoding, "UTF-8"]])
  end

  def test_sax_instruct_loose
    parse_compare(%{<? xml
version = "1.0"
encoding = "UTF-8" ?>},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:attr, :encoding, "UTF-8"]])
  end

  def test_sax_element_simple
    parse_compare(%{<top/>},
                  [[:start_element, :top],
                   [:end_element, :top]])
  end

  def test_sax_element_attrs
    parse_compare(%{<top x="57" y="42"/>}, 
                  [[:start_element, :top],
                   [:attr, :x, "57"],
                   [:attr, :y, "42"],
                   [:end_element, :top]])
  end

  def test_sax_two_top
    parse_compare(%{<top/><top/>},
                  [[:start_element, :top],
                   [:end_element, :top],
                   [:error, "invalid format, multiple top level elements", 1, 9],
                   [:start_element, :top],
                   [:end_element, :top]])


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
                   [:start_element, :top],
                   [:start_element, :child],
                   [:start_element, :grandchild],
                   [:end_element, :grandchild],
                   [:end_element, :child],
                   [:end_element, :top],
                  ])
  end

  def test_sax_nested1_tight
    parse_compare(%{<?xml version="1.0"?><top><child><grandchild/></child></top>},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
                   [:start_element, :top],
                   [:start_element, :child],
                   [:start_element, :grandchild],
                   [:end_element, :grandchild],
                   [:end_element, :child],
                   [:end_element, :top],
                  ])
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
                   [:start_element, :top],
                   [:start_element, :child],
                   [:start_element, :grandchild],
                   [:end_element, :grandchild],
                   [:error, "invalid format, element start and end names do not match", 5, 12]
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
                   [:error, "invalid format, element not terminated", 4, 1]
                  ])
  end

  def test_sax_text
    parse_compare(%{<top>This is some text.</top>},
                  [[:start_element, :top],
                   [:text, "This is some text."],
                   [:end_element, :top]
                  ])
  end

  def test_sax_special
    parse_compare(%{<top name="A&amp;Z">This is &lt;some&gt; text.</top>},
                  [[:start_element, :top],
                   [:attr, :name, 'A&Z'],
                   [:text, "This is <some> text."],
                   [:end_element, :top]
                  ], AllSax, true)
  end

  def test_sax_text_no_term
    parse_compare(%{<top>This is some text.},
                  [[:start_element, :top],
                   [:error, "invalid format, text terminated unexpectedly", 1, 24],
                  ])
  end
  # TBD invalid chacters in text

  def test_sax_doctype
    parse_compare(%{<?xml version="1.0"?>
<!DOCTYPE top PUBLIC "top.dtd">
<top/>
},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
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
                   [:start_element, :top],
                   [:end_element, :top],
                   [:error, "invalid format, DOCTYPE can not come after an element", 3, 11],
                   [:doctype, ' top PUBLIC "top.dtd"']])
  end
  
  def test_sax_instruct_bad_order
    parse_compare(%{
<!DOCTYPE top PUBLIC "top.dtd">
<?xml version="1.0"?>
<top/>
},
                  [[:doctype, " top PUBLIC \"top.dtd\""],
                   [:error, "invalid format, instruction must come before elements", 3, 3],
                   [:instruct, "xml"],
                   [:attr, :version, "1.0"],
                   [:start_element, :top],
                   [:end_element, :top]])
  end
  
  def test_sax_comment
    parse_compare(%{<?xml version="1.0"?>
<!--First comment.-->
<top>Before<!--Nested comment.-->After</top>
},
                  [[:instruct, 'xml'],
                   [:attr, :version, "1.0"],
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
                   [:error, "invalid format, comment terminated unexpectedly", 3, 1], # continue on
                   [:comment, 'First comment.'],
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
                   [:start_element, :top],
                   [:error, "invalid format, cdata terminated unexpectedly", 5, 1]])
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
                   [:instruct, "ox"],
                   [:attr, :version, "1.0"],
                   [:attr, :mode, "object"],
                   [:attr, :circular, "no"],
                   [:attr, :xsd_date, "no"],
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
                     [:start_element, :top],
                     [:text, 'ピーター'],
                     [:end_element, :top]])
    end
  end

end
