#!/usr/bin/env ruby -wW1
# encoding: UTF-8

$: << '../lib'
$: << '../ext'

if __FILE__ == $0
  while (i = ARGV.index('-I'))
    x,path = ARGV.slice!(i, 2)
    $: << path
  end
end

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

  def start_element(name, attrs)
    @calls << [:start_element, name, attrs]
  end
end

class AllSax < StartSax
  def initialize()
    super
  end

  def instruct(target, attrs)
    @calls << [:instruct, target, attrs]
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

  def parse_compare(xml, expected, handler_class=AllSax)
    handler = handler_class.new()
    input = StringIO.new(xml)
    Ox.sax_parse(handler, input)
    assert_equal(expected, handler.calls)
  end
  
  def test_sax_instruct_simple
    parse_compare(%{<?xml?>}, [[:instruct, 'xml', {}]])
  end

  def test_sax_instruct_blank
    parse_compare(%{<?xml?>}, [], StartSax)
  end

  def test_sax_instruct_attrs
    parse_compare(%{<?xml version="1.0" encoding="UTF-8"?>},
                  [[:instruct, 'xml', {'version' => '1.0', 'encoding' => 'UTF-8'}]])
  end

  def test_sax_instruct_loose
    parse_compare(%{<? xml
version = "1.0"
encoding = "UTF-8" ?>},
                  [[:instruct, 'xml', {'version' => '1.0', 'encoding' => 'UTF-8'}]])
  end

  def test_sax_element_simple
    parse_compare(%{<top/>},
                  [[:start_element, 'top', {}],
                   [:end_element, 'top']])
  end

  def test_sax_element_attrs
    parse_compare(%{<top x="57" y="42"/>}, 
                  [[:start_element, 'top', {'x' => '57', 'y' => '42'}],
                   [:end_element, 'top']])
  end

  def test_sax_nested1
    parse_compare(%{<?xml version="1.0"?>
<top>
  <child>
    <grandchild/>
  </child>
</top>
},
                  [[:instruct, 'xml', {'version' => '1.0'}],
                   [:start_element, 'top', {}],
                   [:start_element, 'child', {}],
                   [:start_element, 'grandchild', {}],
                   [:end_element, 'grandchild'],
                   [:end_element, 'child'],
                   [:end_element, 'top'],
                  ])
  end

  def test_sax_nested1_tight
    parse_compare(%{<?xml version="1.0"?><top><child><grandchild/></child></top>},
                  [[:instruct, 'xml', {'version' => '1.0'}],
                   [:start_element, 'top', {}],
                   [:start_element, 'child', {}],
                   [:start_element, 'grandchild', {}],
                   [:end_element, 'grandchild'],
                   [:end_element, 'child'],
                   [:end_element, 'top'],
                  ])
  end

  def test_sax_element_name_mismatch
    parse_compare(%{<?xml version="1.0"?><top><child><grandchild/></parent></top>},
                  [[:instruct, 'xml', {'version' => '1.0'}],
                   [:start_element, 'top', {}],
                   [:start_element, 'child', {}],
                   [:start_element, 'grandchild', {}],
                   [:end_element, 'grandchild'],
                   [:error, "invalid format, element start and end names do not match", 0, 0]
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
                  [[:instruct, 'xml', {'version' => '1.0'}],
                   [:start_element, 'top', {}],
                   [:start_element, 'child', {}],
                   [:start_element, 'grandchild', {}],
                   [:end_element, 'grandchild'],
                   [:end_element, 'child'],
                   [:start_element, 'child', {}],
                   [:start_element, 'grandchild', {}],
                   [:end_element, 'grandchild'],
                   [:start_element, 'grandchild', {}],
                   [:end_element, 'grandchild'],
                   [:end_element, 'child'],
                   [:end_element, 'top'],
                  ])
  end

  def test_sax_text
    parse_compare(%{<top>This is some text.</top>},
                  [[:start_element, "top", {}],
                   [:text, "This is some text."],
                   [:end_element, "top"]
                  ])
  end

  # TBD mix of elements, text, and attributes - tight and loose
  # TBD comments
  # TBD doctype
  # TBD cdata
  # TBD 
  # TBD 
  # TBD read invalid xml

  def xtest_sax_io_pipe
    handler = AllSax.new()
    input,w = IO.pipe
    w << %{<top/>}
    w.close
    Ox.sax_parse(handler, input)
    assert_equal(handler.calls, [[:start_element, 'top', {}], [:end_element, 'top']])
  end

  def xtest_sax_io_file
    handler = AllSax.new()
    input = IO.open(IO.sysopen('basic.xml'))
    Ox.sax_parse(handler, input)
    input.close
    Ox.sax_parse(handler, input)
    assert_equal(handler.calls, [[:start_element, 'top', {}], [:end_element, 'top']])
  end

end

