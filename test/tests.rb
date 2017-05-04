#!/usr/bin/env ruby
# encoding: UTF-8

# Ubuntu does not accept arguments to ruby when called using env. To get warnings to show up the -w options is
# required. That can be set in the RUBYOPT environment variable.
# export RUBYOPT=-w

$VERBOSE = true

$: << File.join(File.dirname(__FILE__), "../lib")
$: << File.join(File.dirname(__FILE__), "../ext")

require 'rubygems' if RUBY_VERSION.start_with?('1.8.')
require 'minitest'
require 'minitest/autorun'
require 'optparse'
require 'date'
require 'bigdecimal'
require 'ox'

$ruby = RUBY_DESCRIPTION.split(' ')[0]
$ruby = 'ree' if 'ruby' == $ruby && RUBY_DESCRIPTION.include?('Ruby Enterprise Edition')

$indent = 2

opts = OptionParser.new
# TBD add indent
opts.on("-h", "--help", "Show this display")                { puts opts; Process.exit!(0) }
opts.parse(ARGV)

$ox_object_options = {
  :encoding=>nil,
  :margin=>'',
  :indent=>2,
  :trace=>0,
  :with_dtd=>false,
  :with_xml=>false,
  :with_instructions=>false,
  :circular=>false,
  :xsd_date=>false,
  :mode=>:object,
  :symbolize_keys=>true,
  :skip=>:skip_white,
  :smart=>false,
  :convert_special=>true,
  :effort=>:strict,
  :invalid_replace=>'',
  :strip_namespace=>false,
  :overlay=>nil,
}

$ox_generic_options = {
  :encoding=>nil,
  :margin=>'',
  :indent=>2,
  :trace=>0,
  :with_dtd=>false,
  :with_xml=>false,
  :with_instructions=>false,
  :circular=>false,
  :xsd_date=>false,
  :mode=>:generic,
  :symbolize_keys=>true,
  :skip=>:skip_white,
  :smart=>false,
  :convert_special=>true,
  :effort=>:strict,
  :invalid_replace=>'',
  :strip_namespace=>false,
  :overlay=>nil,
}

class Func < ::Minitest::Test

  unless respond_to?(:assert_raise)
    alias assert_raise assert_raises
  end

  def test_get_options
    Ox::default_options = $ox_object_options
    opts = Ox.default_options()
    assert_equal(opts, $ox_object_options)
  end

  def test_set_options
    Ox::default_options = $ox_object_options
    o2 = {
      :encoding=>"UTF-8",
      :margin=>'zz',
      :indent=>4,
      :trace=>1,
      :with_dtd=>true,
      :with_xml=>false,
      :with_instructions=>true,
      :circular=>true,
      :xsd_date=>true,
      :mode=>:object,
      :symbolize_keys=>true,
      :skip=>:skip_return,
      :smart=>true,
      :convert_special=>false,
      :effort=>:tolerant,
      :invalid_replace=>'*',
      :strip_namespace=>'spaced',
      :overlay=>nil,
    }
    o3 = { :xsd_date=>false }
    Ox.default_options = o2
    opts = Ox.default_options()
    assert_equal(o2, opts);
    Ox.default_options = o3 # see if it throws an exception
    Ox::default_options = $ox_object_options
  end

  def test_nil
    Ox::default_options = $ox_object_options
    dump_and_load(nil, false)
  end

  def test_true
    Ox::default_options = $ox_object_options
    dump_and_load(true, false)
  end

  def test_false
    Ox::default_options = $ox_object_options
    dump_and_load(false, false)
  end

  def test_fixnum
    Ox::default_options = $ox_object_options
    dump_and_load(7, false)
    dump_and_load(-19, false)
    dump_and_load(0, false)
  end

  def test_float
    Ox::default_options = $ox_object_options
    dump_and_load(7.7, false)
    dump_and_load(-1.9, false)
    dump_and_load(0.0, false)
    dump_and_load(-10.000005, false)
    dump_and_load(1.000000000005, false)
  end

  def test_string
    Ox::default_options = $ox_object_options
    dump_and_load('a string', false)
    dump_and_load('', false)
  end

  def test_symbol
    Ox::default_options = $ox_object_options
    dump_and_load(:a_symbol, false)
    dump_and_load(:<=, false)
  end

  def test_base64
    Ox::default_options = $ox_object_options
    dump_and_load('a & x', false)
  end

  def test_time
    Ox::default_options = $ox_object_options
    t = Time.local(2012, 1, 5, 23, 58, 7)
    dump_and_load(t, false)
  end

  def test_date
    Ox::default_options = $ox_object_options
    dump_and_load(Date.new(2011, 1, 5), false)
  end

  def test_array
    Ox::default_options = $ox_object_options
    dump_and_load([], false)
    dump_and_load([1, 'a'], false)
  end

  def test_hash
    Ox::default_options = $ox_object_options
    dump_and_load({ }, false)
    dump_and_load({ 'a' => 1, 2 => 'b' }, false)
  end

  def test_range
    Ox::default_options = $ox_object_options
    if RUBY_VERSION.start_with?('1.8')
      assert(true)
    else
      dump_and_load((0..3), false)
      dump_and_load((-2..3.7), false)
      dump_and_load(('a'...'f'), false)
      t = Time.local(2012, 1, 5, 23, 58, 7)
      t2 = t + 20
      dump_and_load((t..t2), false)
    end
  end

  def test_regex
    Ox::default_options = $ox_object_options
    if RUBY_VERSION.start_with?('1.8') || 'rubinius' == $ruby
      assert(true)
    else
      dump_and_load(/^[0-9]/ix, false)
      dump_and_load(/^[&0-9]/ix, false) # with xml-unfriendly character
    end
  end

  def test_bignum
    Ox::default_options = $ox_object_options
    dump_and_load(7 ** 55, false)
    dump_and_load(-7 ** 55, false)
  end

  def test_bigdecimal
    Ox::default_options = $ox_object_options
    bd = BigDecimal.new('7.123456789')
    dump_and_load(bd, false)
  end

  def test_complex_number
    Ox::default_options = $ox_object_options
    if RUBY_VERSION.start_with?('1.8') || 'rubinius' == $ruby
      assert(true)
    else
      dump_and_load(Complex(1), false)
      dump_and_load(Complex(3, 2), false)
    end
  end

  def test_rational
    Ox::default_options = $ox_object_options
    if RUBY_VERSION.start_with?('1.8')
      assert(true)
    else
      dump_and_load(Rational(1, 3), false)
      dump_and_load(Rational(0, 3), false)
    end
  end

  def test_object
    Ox::default_options = $ox_object_options
    dump_and_load(Bag.new({ }), false)
    dump_and_load(Bag.new(:@x => 3), false)
  end
  
  def test_bad_object
    Ox::default_options = $ox_object_options
    xml = %{<?xml version="1.0"?>
<o c="Bad::Boy">
  <i a="@x">3</i>
</o>
}
    xml2 = %{<?xml version="1.0"?>
<o c="Bad">
  <i a="@x">7</i>
</o>
}
    assert_raise(NameError) {
      Ox.load(xml, :mode => :object, :trace => 0)
    }
    loaded = Ox.load(xml, :mode => :object, :trace => 0, :effort => :tolerant)
    assert_nil(loaded)
    loaded = Ox.load(xml, :mode => :object, :trace => 0, :effort => :auto_define)
    assert_equal(loaded.class.to_s, 'Bad::Boy')
    assert_equal(loaded.class.superclass.to_s, 'Ox::Bag')
    loaded = Ox.load(xml2, :mode => :object, :trace => 0, :effort => :auto_define)
    assert_equal(loaded.class.to_s, 'Bad')
    assert_equal(loaded.class.superclass.to_s, 'Ox::Bag')
  end

  def test_bad_class
    Ox::default_options = $ox_object_options
    xml = %{<?xml version="1.0"?>
<o c="Bad:Boy">
  <i a="@x">3</i>
</o>
}
    assert_raise(Ox::ParseError) {
      Ox.load(xml, :mode => :object, :trace => 0)
    }
  end

  def test_empty_element
    Ox::default_options = $ox_object_options
    xml = %{<?xml version="1.0"?>
<top>
  <>
</top>}
    assert_raise(Ox::ParseError) {
      Ox.load(xml, :mode => :object, :trace => 0)
    }
  end

  def test_xml_instruction_format
    Ox::default_options = $ox_object_options
    xml = %{<?xml version="1.0" ?>
<s>Test</s>
}
    loaded = Ox.load(xml, :mode => :object)
    assert_equal('Test', loaded);
  end

  def test_xml_instruction
    Ox::default_options = $ox_object_options
    xml = Ox.dump("test", :mode => :object, :with_xml => false)
    assert_equal("<s>test</s>\n", xml)
    xml = Ox.dump("test", :mode => :object, :with_xml => true)
    assert_equal("<?xml version=\"1.0\"?>\n<s>test</s>\n", xml)
    xml = Ox.dump("test", :mode => :object, :with_xml => true, :encoding => 'UTF-8')
    assert_equal("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<s>test</s>\n", xml)
  end

  def test_ox_instruction
    Ox::default_options = $ox_object_options
    xml = Ox.dump("test", :mode => :object, :with_xml => true, :with_instructions => true)
    assert_equal("<?xml version=\"1.0\"?>\n<?ox version=\"1.0\" mode=\"object\" circular=\"no\" xsd_date=\"no\"?>\n<s>test</s>\n", xml)
    xml = Ox.dump("test", :mode => :object, :with_instructions => true)
    assert_equal("<?ox version=\"1.0\" mode=\"object\" circular=\"no\" xsd_date=\"no\"?>\n<s>test</s>\n", xml)
    xml = Ox.dump("test", :mode => :object, :with_instructions => true, :circular => true, :xsd_date => true)
    assert_equal("<?ox version=\"1.0\" mode=\"object\" circular=\"yes\" xsd_date=\"yes\"?>\n<s i=\"1\">test</s>\n", xml)
    xml = Ox.dump("test", :mode => :object, :with_instructions => true, :circular => false, :xsd_date => false)
    assert_equal("<?ox version=\"1.0\" mode=\"object\" circular=\"no\" xsd_date=\"no\"?>\n<s>test</s>\n", xml)
  end

  def test_embedded_instruction
    Ox::default_options = $ox_object_options
    xml = %{<?xml version="1.0"?><?pro cat="quick"?>
<top>
  <str>This is a <?attrs dog="big"?> string.</str><?content dog is big?>
  <mid><?dog big?></mid>
</top>
}
    doc = Ox.load(xml, :mode => :generic)
    inst = doc.top.str.nodes[1]
    assert_equal(Ox::Instruct, inst.class)
    assert_equal('attrs', inst.target)
    assert_nil(inst.content)
    assert_equal({:dog=>'big'}, inst.attributes)

    inst = doc.top.content
    assert_equal(Ox::Instruct, inst.class)
    assert_equal('content', inst.target)
    assert_equal(' dog is big', inst.content)
    assert_equal({}, inst.attributes)
    x = Ox.dump(doc, :with_xml => true)
    assert_equal(xml, x)
  end

  def test_dtd
    Ox::default_options = $ox_object_options
    xml = Ox.dump("test", :mode => :object, :with_dtd => true)
    assert_equal("<!DOCTYPE s SYSTEM \"ox.dtd\">\n<s>test</s>\n", xml)
  end

  def test_lone_dtd
    Ox::default_options = $ox_object_options
    xml = "<!DOCTYPE html>" # not really a valid xml but should pass anyway
    doc = Ox.parse(xml)
    assert_equal('html', doc.nodes[0].value)
  end

  def test_big_dtd
    Ox::default_options = $ox_object_options
    xml = %{<!DOCTYPE Objects [<!ELEMENT Objects (RentObjects)><!ENTITY euml "&#235;"><!ENTITY Atilde "&#195;">]>}
    doc = Ox.parse(xml)
    assert_equal(%{Objects [<!ELEMENT Objects (RentObjects)><!ENTITY euml "&#235;"><!ENTITY Atilde "&#195;">]}, doc.nodes[0].value)
  end

  def test_quote_value
    Ox::default_options = $ox_object_options
    xml = %{<top name="Pete"/>}
    doc = Ox.parse(xml)
    assert_equal('Pete', doc.attributes[:name])
  end

  def test_escape_value
    Ox::default_options = $ox_object_options
    xml = %{\n<top name="<&amp;test>">&lt;not \'quoted\'&gt;</top>\n}
    doc = Ox.parse(xml)
    assert_equal('<&test>', doc.attributes[:name])
    dumped_xml = Ox.dump(doc)
    assert_equal(xml, dumped_xml)
  end

  def test_escape_bad
    Ox::default_options = $ox_object_options
    xml = %{<top name="&example;">&example;</top>}
    begin
      doc = Ox.parse(xml)
    rescue Exception => e
      assert_equal('Invalid format, invalid special character sequence at line 1, column 22 ', e.message.split('[')[0])
      return
    end
    assert(false)
  end

  def test_escape_ignore
    Ox::default_options = $ox_generic_options
    xml = %{<top name="&example;">&example;</top>}
    doc = Ox.load(xml, :convert_special => false)
    assert_equal('&example;', doc.attributes[:name])
    assert_equal('&example;', doc.nodes[0])
  end

  def test_escape_hex_value
    Ox::default_options = $ox_object_options
    xml = %{\n<top name="&lt;&#x40;test&gt;"/>\n}
    doc = Ox.parse(xml)
    assert_equal('<@test>', doc.attributes[:name])
  end

  def test_escape_dump_tolerant
    Ox::default_options = $ox_object_options
    dumped_xml = Ox.dump("tab\tamp&backspace\b.", :effort => :tolerant)
    assert_equal("<s>tab\tamp&amp;backspace.</s>\n", dumped_xml)
  end

  def test_escape_dump_strict
    Ox::default_options = $ox_object_options
    begin
      Ox.dump("tab\tamp&backspace\b.", :effort => :strict)
    rescue Exception => e
      assert_equal("'\\#x08' is not a valid XML character.", e.message)
      return
    end
    assert(false)
  end

  def test_escape_dump_replace
    Ox::default_options = $ox_object_options
    dumped_xml = Ox.dump("tab\tamp&backspace\b.", :effort => :tolerant, :invalid_replace => '#')
    assert_equal("<s>tab\tamp&amp;backspace#.</s>\n", dumped_xml)
  end

  def test_escape_dump_no_replace
    Ox::default_options = $ox_object_options
    dumped_xml = Ox.dump("tab\tamp&backspace\b.", :effort => :tolerant, :invalid_replace => nil)
    assert_equal("<s>tab\tamp&amp;backspace&#x0008;.</s>\n", dumped_xml)
  end

  def test_escape_load_tolerant
    Ox::default_options = $ox_object_options
    obj = Ox.load("<s>tab\tamp&amp;backspace\b.</s>", :effort => :tolerant, :skip => :skip_none)
    assert_equal("tab\tamp&backspace\b.", obj)
  end

  def test_escape_load_strict
    Ox::default_options = $ox_object_options
    begin
      Ox.load("<s>tab\tamp&amp;backspace\b.</s>", :effort => :strict)
    rescue Exception => e
      assert_equal("invalid character at line 1, column 26 ", e.message.split('[')[0])
      return
    end
    assert(false)
  end

  def test_escape_utf8_value
    Ox::default_options = $ox_object_options
    xml = %{<?xml encoding="UTF-8"?>\n<top name="&#x30d4;&#12540;&#xE382BFE383BC;">&#xe38394; test &#64;</top>\n}
    doc = Ox.parse(xml).root()
    assert_equal('ピ test @', doc.nodes[0])
    assert_equal('ピーター', doc.attributes[:name])
  end

  def test_escape_utf8_nil_encoding
    Ox::default_options = $ox_object_options
    xml = %{<?xml?>\n<top name="&#x30d4;&#12540;&#xE382BFE383BC;">&#x30D4; test &#64;</top>\n}
    doc = Ox.parse(xml).root()
    assert_equal('ピ test @', doc.nodes[0])
    assert_equal('ピーター', doc.attributes[:name])
  end

  def test_escape_bom_utf8_encoding
    Ox::default_options = $ox_object_options
    xml = %{\xEF\xBB\xBF<?xml?>\n<top name="bom"></top>\n}
    doc = Ox.parse(xml).root()
    assert_equal('bom', doc.attributes[:name])
    unless RUBY_VERSION.start_with?('1.8')# || 'rubinius' == $ruby
      assert_equal('UTF-8', doc.attributes[:name].encoding.to_s)
    end
  end

  def test_escape_bom_bad_encoding
    Ox::default_options = $ox_object_options
    xml = %{\xEF\xBB<?xml?>\n<top name="bom"></top>\n}
    assert_raise(Ox::ParseError) {
      Ox.parse(xml).root()
    }
  end

  def test_attr_as_string
    Ox::default_options = $ox_object_options
    xml = %{<top name="Pete"/>}
    doc = Ox.load(xml, :mode => :generic, :symbolize_keys => false)
    assert_equal(['name'], doc.attributes.keys)
  end

  def test_single_quote
    Ox::default_options = $ox_generic_options
    xml = %{<top name='Pete'/>}
    doc = Ox.load(xml, :effort => :tolerant)
    assert_equal('Pete', doc.attributes[:name])
  end

  def test_no_quote
    Ox::default_options = $ox_generic_options
    xml = %{<top name=Pete />}
    doc = Ox.load(xml, :effort => :tolerant)
    assert_equal('Pete', doc.attributes[:name])
  end

  def test_skip_none
    Ox::default_options = $ox_object_options
    xml = %{<top>  Pete\r\n  Ohler</top>}
    doc = Ox.load(xml, :mode => :generic, :symbolize_keys => false, :skip => :skip_none)
    x2 = Ox.dump(doc, :indent => 0)
    assert_equal(%{\n<top>  Pete\r\n  Ohler</top>\n}, x2)
  end

  def test_skip_return
    Ox::default_options = $ox_object_options
    xml = %{<top>  Pete\r\n  Ohler</top>}
    doc = Ox.load(xml, :mode => :generic, :symbolize_keys => false, :skip => :skip_return)
    x2 = Ox.dump(doc)
    assert_equal(%{\n<top>  Pete\n  Ohler</top>\n}, x2)
  end

  def test_skip_space
    Ox::default_options = $ox_object_options
    xml = %{<top>  Pete\r\n  Ohler</top>}
    doc = Ox.load(xml, :mode => :generic, :symbolize_keys => false, :skip => :skip_white)
    x2 = Ox.dump(doc)
    assert_equal(%{\n<top> Pete Ohler</top>\n}, x2)
  end

  def test_tolerant
    Ox::default_options = $ox_generic_options
    Ox::default_options = { :skip => :skip_return }
    xml = %{<!doctype HTML>
<html lang=en>
  <head garbage='trash'>
    <bad attr="some&#xthing">
    <bad alone>
  </head>
  <body>
  This is a test of the &tolerant&#x effort option.
  </body>
</html>
<ps>after thought</ps>
}
    expected = %{
<!DOCTYPE HTML >
<html lang="en">
  <head garbage="trash">
    <bad attr="some&amp;#xthing">
      <bad alone=""/>
    </bad>
  </head>
  <body>
  This is a test of the &amp;tolerant&amp;#x effort option.
  </body>
</html>
<ps>after thought</ps>
}
    doc = Ox.load(xml, :effort => :tolerant)
    #puts Ox.dump(doc)
    assert_equal(expected, Ox.dump(doc, :with_xml => false))
  end

  def test_tolerant_case
    Ox::default_options = $ox_generic_options
    xml = %{<!doctype HTML>
<html lang=en>
  <head>
    <nonsense/>
  </HEAD>
  <Body>
    Some text.
  </bODY>
</hTml>
}
    expected = %{
<!DOCTYPE HTML >
<html lang="en">
  <head>
    <nonsense/>
  </head>
  <Body> Some text. </Body>
</html>
}
    doc = Ox.load(xml, :effort => :tolerant)
    #puts Ox.dump(doc)
    assert_equal(expected, Ox.dump(doc, :with_xml => false))
  end

  def test_class
    Ox::default_options = $ox_object_options
    dump_and_load(Bag, false)
  end

  def test_exception
    Ox::default_options = $ox_object_options
    if RUBY_VERSION.start_with?('1.8') || 'rubinius' == $ruby
      assert(true)
    else
      e = StandardError.new("Some Error")
      e.set_backtrace(["./func.rb:119: in test_exception",
                       "./fake.rb:57: in fake_func"])
      dump_and_load(e, false)
    end
  end

  def test_exception_bag
    Ox::default_options = $ox_object_options
    if RUBY_VERSION.start_with?('1.8') || 'rubinius' == $ruby
      assert(true)
    else
      xml = %{
<e c="FakeError">
  <s a="mesg">Some Error</s>
  <a a="bt">
    <s>./func.rb:119: in test_exception</s>
    <s>./fake.rb:57: in fake_func</s>
  </a>
</e>
}
      x = Ox.load(xml, :mode => :object, :effort => :auto_define)
      assert_equal('Some Error', x.message())
      assert(x.is_a?(Exception))
    end
  end

  def test_struct
    Ox::default_options = $ox_object_options
    if 'ruby' == $ruby || 'ree' == $ruby
      s = Struct.new('Box', :x, :y, :w, :h)
      dump_and_load(s.new(2, 4, 10, 20), false)
    else
      assert(true)
    end
  end

  def test_bad_format
    Ox::default_options = $ox_object_options
    xml = "<?xml version=\"1.0\"?>\n<tag>test</tagz>\n"
    assert_raise(Ox::ParseError) {
      Ox.load(xml, :mode => :generic, :trace => 0)
    }
  end

  def test_array_multi
    Ox::default_options = $ox_object_options
    t = Time.local(2012, 1, 5, 23, 58, 7)
    if RUBY_VERSION.start_with?('1.8')
      dump_and_load([nil, true, false, 3, 'z', 7.9, 'a&b', :xyz, t], false)
    else
      dump_and_load([nil, true, false, 3, 'z', 7.9, 'a&b', :xyz, t, (-1..7)], false)
    end
  end

  def test_hash_multi
    Ox::default_options = $ox_object_options
    t = Time.local(2012, 1, 5, 23, 58, 7)
    if RUBY_VERSION.start_with?('1.8')
      dump_and_load({ nil => nil, true => true, false => false, 3 => 3, 'z' => 'z', 7.9 => 7.9, 'a&b' => 'a&b', :xyz => :xyz, t => t }, false)
    else
      dump_and_load({ nil => nil, true => true, false => false, 3 => 3, 'z' => 'z', 7.9 => 7.9, 'a&b' => 'a&b', :xyz => :xyz, t => t, (-1..7) => (-1..7) }, false)
    end
  end

  def test_object_multi
    Ox::default_options = $ox_object_options
    t = Time.local(2012, 1, 5, 23, 58, 7)
    if RUBY_VERSION.start_with?('1.8')
      dump_and_load(Bag.new(:@a => nil, :@b => true, :@c => false, :@d => 3, :@e => 'z', :@f => 7.9, :@g => 'a&b', :@h => :xyz, :@i => t), false)
    else
      dump_and_load(Bag.new(:@a => nil, :@b => true, :@c => false, :@d => 3, :@e => 'z', :@f => 7.9, :@g => 'a&b', :@h => :xyz, :@i => t, :@j => (-1..7)), false)
    end
  end

  def test_complex
    Ox::default_options = $ox_object_options
    dump_and_load(Bag.new(:@o => Bag.new(:@a => [2]), :@a => [1, {:b => 3, :a => [5], :c => Bag.new(:@x => 7)}]), false)
  end

  def test_dump_margin
    Ox::default_options = $ox_object_options
    x = Ox.dump(Bag.new(:@o => Bag.new(:@a => [2]), :@a => [1, {:b => 3, :a => [5], :c => Bag.new(:@x => 7)}]), :indent => 1, :margin => '##')

    assert_equal('##<o c="Bag">
## <a a="@a">
##  <i>1</i>
##  <h>
##   <m>b</m>
##   <i>3</i>
##   <m>a</m>
##   <a>
##    <i>5</i>
##   </a>
##   <m>c</m>
##   <o c="Bag">
##    <i a="@x">7</i>
##   </o>
##  </h>
## </a>
## <o a="@o" c="Bag">
##  <a a="@a">
##   <i>2</i>
##  </a>
## </o>
##</o>
', x)
  end

  # Create an Object and an Array with the same Objects in them. Dump and load
  # and then change the ones in the loaded Object to verify that the ones in
  # the array change in the same way. They are the same objects so they should
  # change. Perform the operation on both the object before and the loaded so
  # the equal() method can be used.
  def test_circular
    Ox::default_options = $ox_object_options
    if RUBY_VERSION.start_with?('1.8')
      # In 1.8.7 the eql? method behaves differently but the results are
      # correct when viewed by eye.
      assert(true)
    else
      a = [1]
      s = "a,b,c"
      h = { 1 => 2 }
      e = Ox::Element.new('Zoo')
      e[:cage] = 'bear'
      b = Bag.new(:@a => a, :@s => s, :@h => h, :@e => e)
      a << s
      a << h
      a << e
      a << b
      loaded = dump_and_load(b, false, true)
      # modify the string
      loaded.instance_variable_get(:@s).gsub!(',', '_')
      b.instance_variable_get(:@s).gsub!(',', '_')
      # modify hash
      loaded.instance_variable_get(:@h)[1] = 3
      b.instance_variable_get(:@h)[1] = 3
      # modify Element
      loaded.instance_variable_get(:@e)['pen'] = 'zebra'
      b.instance_variable_get(:@e)['pen'] = 'zebra'
      # pp loaded
      assert_equal(b, loaded)
    end
  end

  # verify that an exception is raised if a circular ref object is created.
  def test_circular_limit
    Ox::default_options = $ox_object_options
    h = {}
    h[:h] = h
    begin
      Ox.dump(h)
    rescue Exception
      assert(true)
      return
    end
    assert(false)
  end

  def test_raw
    Ox::default_options = $ox_object_options
    raw = Ox::Element.new('raw')
    su = Ox::Element.new('sushi')
    su[:kind] = 'fish'
    raw << su
    ba = Ox::Element.new('basashi')
    ba[:kind] = 'animal'
    raw << ba
    dump_and_load(Bag.new(:@raw => raw), false)
  end

  def test_nameerror
    Ox::default_options = $ox_object_options
    begin
      "x".foo
    rescue Exception => e
      xml = Ox.dump(e, :effort => :tolerant)
      o = Ox.load(xml, :mode => :object)
      xml2 = Ox.dump(o, :effort => :tolerant)
      assert_equal(xml, xml2)
      return
    end
    assert(false)
  end
  
  def test_mutex
    Ox::default_options = $ox_object_options
    if defined?(Mutex) && 'rubinius' != $ruby
      # Mutex can not be serialize but it should not raise an exception.
      xml = Ox.dump(Mutex.new, :indent => 2, :effort => :tolerant)
      assert_equal(%{<z/>
}, xml)
      xml = Ox.dump(Bag.new(:@x => Mutex.new), :indent => 2, :effort => :tolerant)
      assert_equal(%{<o c="Bag">
  <z a="@x"/>
</o>
}, xml)
    else
      assert(true)
    end
  end

  def test_encoding
    Ox::default_options = $ox_object_options
    if RUBY_VERSION.start_with?('1.8')# || 'rubinius' == $ruby
      assert(true)
    else
      s = 'ピーター'
      xml = Ox.dump(s, :with_xml => true, :encoding => 'UTF-8')
      #puts xml
      #puts xml.encoding.to_s
      assert_equal('UTF-8', xml.encoding.to_s)
      obj = Ox.load(xml, :mode => :object)
      assert_equal(s, obj)
    end
  end

  def test_full_encoding
    Ox::default_options = $ox_generic_options
    if RUBY_VERSION.start_with?('1.8') || 'rubinius' == $ruby
      assert(true)
    else
      xml = %{<?xml version="1.0" encoding="UTF-8"?>
<いち name="ピーター" つま="まきえ">ピーター</いち>
}
      obj = Ox.load(xml)
      dumped = Ox.dump(obj, :with_xml => true)
      assert_equal('UTF-8', dumped.encoding.to_s)
      assert_equal(xml, dumped)
    end
  end

  def test_obj_encoding
    Ox::default_options = $ox_object_options
    if RUBY_VERSION.start_with?('1.8')
      # jruby supports in a non standard way and does not allow utf-8 encoded object attribute names
      assert(true)
    else
      orig = Ox.default_options
      opts = orig.clone
      opts[:encoding] = 'UTF-8'
      opts[:with_xml] = true
      Ox.default_options = opts
      begin
        # only 1.9.x rubies know how to parse a UTF-8 symbol
        dump_and_load(Bag.new('@tsuma'.to_sym => 'まきえ'.to_sym), false)
        dump_and_load(Bag.new('@つま'.to_sym => 'まきえ'.to_sym), false)
      ensure
        Ox.default_options = orig
      end
    end
  end

  def test_instructions
    Ox::default_options = $ox_object_options
    xml = Ox.dump("test", :with_instructions => true)
    #puts xml
    obj = Ox.load(xml) # should convert it to an object
    assert_equal("test", obj)
  end

  def test_generic_string
    Ox::default_options = $ox_object_options
    xml = %{<?xml?>
<Str>A &lt;boo&gt;</Str>
}
    doc = Ox.load(xml, :mode => :generic)
    xml2 = Ox.dump(doc, :with_xml => true)
    assert_equal(xml, xml2)
  end

  def test_generic_white_string
    Ox::default_options = $ox_generic_options
    xml = %{<?xml?>
<Str> </Str>
}
    doc = Ox.load(xml, :skip => :skip_none)
    assert_equal(' ', doc.root.nodes[0])
  end

  def test_generic_encoding
    Ox::default_options = $ox_object_options
    if RUBY_VERSION.start_with?('1.8')
      assert(true)
    else
      xml = %{<?xml encoding="UTF-8"?>
<Str>&lt;まきえ&gt;</Str>
}
      doc = Ox.load(xml, :mode => :generic)
      xml2 = Ox.dump(doc, :with_xml => true)
      assert_equal(xml, xml2)
    end
  end

  def test_IO
    Ox::default_options = $ox_object_options
    f = File.open(__FILE__, "r")
    assert_raise(NotImplementedError) {
      Ox.dump(f, :effort => :strict)
    }
    xml = Ox.dump(f, :effort => :tolerant)
    obj = Ox.load(xml, :mode => :object) # should convert it to an object
    assert_nil(obj)
  end
  
  def locate_xml()
    %{<?xml?>
<Family real="false">
  <Pete age="57" type="male">
    <Kid1 age="32"/><!--Nicole-->
    <Kid2 age="31"/><!--Pamela-->
  </Pete>
</Family>
<!--One Only-->
}
  end
  
  def test_locate_self
    Ox::default_options = $ox_object_options
    doc = Ox.parse(locate_xml)
    nodes = doc.locate(nil)
    assert_equal(doc, nodes[0])
  end

  def test_locate_top
    Ox::default_options = $ox_object_options
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('Family')
    assert_equal([doc.nodes[0]], nodes)
  end

  def test_locate_top_wild
    Ox::default_options = $ox_object_options
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('?')
    assert_equal(doc.nodes[0], nodes[0])
  end

  def test_locate_child
    Ox::default_options = $ox_object_options
    doc = Ox.parse(locate_xml)

    nodes = doc.locate('Family/?')
    assert_equal(['Pete'], nodes.map { |n| n.value })

    nodes = doc.locate('Family/?/^Element')
    assert_equal(['Kid1', 'Kid2'], nodes.map { |n| n.value })

    nodes = doc.locate('Family/Pete/^Element')
    assert_equal(['Kid1', 'Kid2'], nodes.map { |n| n.value })

    nodes = doc.locate('Family/Makie/?')
    assert_equal([], nodes.map { |n| n.name })
  end

  def test_locate_comment
    Ox::default_options = $ox_object_options
    doc = Ox.parse(locate_xml)

    nodes = doc.locate('^Comment')
    assert_equal(['One Only'], nodes.map { |n| n.value })
  end

  def test_locate_child_wild_wild
    Ox::default_options = $ox_object_options
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('Family/?/?')
    assert_equal(['Kid1', 'Nicole', 'Kid2', 'Pamela'], nodes.map { |n| n.value })
  end

  def test_locate_child_wild
    Ox::default_options = $ox_object_options
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('Family/Pete/?')
    assert_equal(['Kid1', 'Nicole', 'Kid2', 'Pamela'], nodes.map { |n| n.value })
  end

  def test_locate_child_wild_missing
    Ox::default_options = $ox_object_options
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('Family/Makie/?')
    assert_equal([], nodes.map { |n| n.name })
  end

  def test_locate_child_comments
    Ox::default_options = $ox_object_options
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('Family/?/^Comment')
    assert_equal(['Nicole', 'Pamela'], nodes.map { |n| n.value })
  end

  def test_locate_attribute
    Ox::default_options = $ox_object_options
    doc = Ox.parse(locate_xml)

    nodes = doc.locate('Family/@?')
    assert_equal(['false'], nodes)

    nodes = doc.locate('Family/@real')
    assert_equal(['false'], nodes)

    nodes = doc.locate('Family/Pete/@?')
    assert_equal(['57', 'male'], nodes.sort)

    nodes = doc.locate('Family/Pete/@age')
    assert_equal(['57'], nodes)

    nodes = doc.locate('Family/Makie/@?')
    assert_equal([], nodes)

    nodes = doc.locate('Family/Pete/?/@age')
    assert_equal(['31', '32'], nodes.sort)

    nodes = doc.locate('Family/*/@age')
    assert_equal(['31', '32', '57'], nodes.sort)

    nodes = doc.locate('*/@?')
    assert_equal(['31', '32', '57', 'false', 'male'], nodes.sort)

    pete = doc.locate('Family/Pete')[0]
    nodes = pete.locate('*/@age')
    assert_equal(['31', '32', '57'], nodes.sort)

    assert_raise(::Ox::InvalidPath) {
      nodes = doc.locate('Family/@age/?')
    }

    assert_raise(::Ox::InvalidPath) {
      nodes = doc.locate('Family/?[/?')
    }
  end

  def test_locate_qual_index
    Ox::default_options = $ox_object_options
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('Family/Pete/?[0]')
    assert_equal(['Kid1'], nodes.map { |e| e.value } )
    nodes = doc.locate('Family/Pete/?[1]')
    assert_equal(['Nicole'], nodes.map { |e| e.value } )
    nodes = doc.locate('Family/Pete/?[2]')
    assert_equal(['Kid2'], nodes.map { |e| e.value } )
  end

  def test_locate_qual_less
    Ox::default_options = $ox_object_options
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('Family/Pete/?[<1]')
    assert_equal(['Kid1'], nodes.map { |e| e.value } )
  end

  def test_locate_qual_great
    Ox::default_options = $ox_object_options
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('Family/Pete/?[>1]')
    assert_equal(['Kid2', 'Pamela'], nodes.map { |e| e.value } )
  end

  def test_locate_qual_last
    Ox::default_options = $ox_object_options
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('Family/Pete/?[-1]')
    assert_equal(['Pamela'], nodes.map { |e| e.value } )
  end

  def test_locate_qual_last_attr
    Ox::default_options = $ox_object_options
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('Family/Pete/?[-2]/@age')
    assert_equal(['31'], nodes )
  end

  def easy_xml()
    %{<?xml?>
<Family real="false">
  <Pete age="58" type="male">
    <Kid age="33"><!-- comment -->Nicole</Kid>
    <Kid age="31">Pamela</Kid>
  </Pete>
</Family>
}
  end
  
  def test_easy_attribute
    Ox::default_options = $ox_object_options
    doc = Ox.parse(easy_xml)
    assert_equal('false', doc.Family.real)
    assert_equal('58', doc.Family.Pete.age)
  end

  def test_easy_attribute_missing
    Ox::default_options = $ox_object_options
    doc = Ox.parse(easy_xml)
    assert_raise(NoMethodError) {
      doc.Family.fake
    }
  end

  def test_easy_element
    Ox::default_options = $ox_object_options
    doc = Ox.parse(easy_xml)
    assert_equal(::Ox::Element, doc.Family.Pete.class)
    assert_equal('Pete', doc.Family.Pete.name)
    assert_equal('Nicole', doc.Family.Pete.Kid.text)
    assert_nil(doc.Family.Pete.text)
  end

  def test_easy_respond_to
    Ox::default_options = $ox_object_options
    doc = Ox.parse(easy_xml)
    assert_equal(true, doc.respond_to?(:Family))
    assert_equal(true, doc.Family.respond_to?('Pete'))
    assert_equal(false, doc.Family.respond_to?('Fred'))
    assert_equal(true, doc.Family.Pete.respond_to?('age'))
    assert_equal(false, doc.Family.Pete.respond_to?('email'))
  end

  def test_easy_replace_text
    Ox::default_options = $ox_object_options
    doc = Ox.parse(easy_xml)
    k1 = doc.Family.Pete.Kid(1)
    k1.replace_text('Spam')
    assert_equal('Spam', doc.Family.Pete.Kid(1).text)
  end
  
  def test_easy_element_index
    Ox::default_options = $ox_object_options
    doc = Ox.parse(easy_xml)
    assert_equal('Pamela', doc.Family.Pete.Kid(1).text)
  end

  def test_easy_element_missing
    Ox::default_options = $ox_object_options
    doc = Ox.parse(easy_xml)
    assert_raise(NoMethodError) {
      doc.Family.Bob
    }
  end

  def test_arbitrary_raw_xml
    Ox::default_options = $ox_object_options
    root = Ox::Element.new('root')
    root << Ox::Raw.new('<foo>bar</bar>')
    assert_equal("\n<root>\n  <foo>bar</bar>\n</root>\n", Ox.dump(root))
  end

  def test_block
    Ox::default_options = $ox_generic_options
    results = []
    Ox.load('<one>first</one><two>second</two><!--three-->') do |x|
      results << Ox.dump(x)
    end
    assert_equal("\n<one>first</one>\n\n<two>second</two>\n\n<!-- three -->\n", results.join())
  end

  def test_namespace_no_strip
    Ox::default_options = $ox_generic_options
    results = []
    doc = Ox.load('<spaced:one><two spaced:out="no">inside</two></spaced:one>')
    assert_equal(%|
<spaced:one>
  <two spaced:out="no">inside</two>
</spaced:one>
|, Ox.dump(doc))
  end

  def test_namespace_strip_wild
    Ox::default_options = $ox_generic_options
    results = []
    doc = Ox.load('<spaced:one><two spaced:out="no">inside</two></spaced:one>', :strip_namespace => true )
    assert_equal(%|
<one>
  <two out="no">inside</two>
</one>
|, Ox.dump(doc))
  end

  def test_namespace_strip
    Ox::default_options = $ox_generic_options
    Ox::default_options = { :strip_namespace => "spaced" }
    results = []
    doc = Ox.load('<spaced:one><two spaced:out="no" other:space="yes">inside</two></spaced:one>')
    assert_equal(%|
<one>
  <two out="no" other:space="yes">inside</two>
</one>
|, Ox.dump(doc))
  end

  def test_builder
    b = Ox::Builder.new(:indent => 2)
    assert_equal(1, b.line())
    assert_equal(1, b.column())
    assert_equal(0, b.pos())
    b.instruct(:xml, :version => '1.0', :encoding => 'UTF-8')
    assert_equal(1, b.line())
    assert_equal(39, b.column())
    assert_equal(38, b.pos())
    b.element('one', :a => "ack", 'b' => "<ba\"'&ck>")
    b.element('two')
    assert_equal(3, b.line())
    assert_equal(6, b.column())
    assert_equal(88, b.pos())
    b.pop()
    b.comment('just a comment')
    b.element('three')
    b.text(%|my name is Peter & <"ピーター">|)
    b.pop()
    b.cdata("multi\nline\ncdata")
    b.raw("<multi></multi>\n<line></line>\n<raw></raw>")
    b.comment("Multi\nline\ncomment")
    b.close()
    xml = b.to_s
    assert_equal(%|<?xml version="1.0" encoding="UTF-8"?>
<one a="ack" b="&lt;ba&quot;'&amp;ck&gt;">
  <two/>
  <!-- just a comment --/>
  <three>my name is Peter &amp; &lt;"ピーター"&gt;</three>
  <![CDATA[multi
line
cdata]]><multi></multi>
<line></line>
<raw></raw>
  <!-- Multi
line
comment --/>
</one>
|, xml)
  end

  def test_builder_file
    filename = File.join(File.dirname(__FILE__), 'create_file_test.xml')
    b = Ox::Builder.file(filename, :indent => 2)
    b.instruct(:xml, :version => '1.0', :encoding => 'UTF-8')
    b.element('one', :a => "ack", 'b' => 'back')
    b.element('two')
    b.pop()
    b.comment('just a comment')
    b.element('three')
    b.text("my name is \"ピーター\"")
    b.pop()
    b.pop()
    b.close()
    
    xml = File.read(filename)
    xml.force_encoding('UTF-8')
    assert_equal(%|<?xml version="1.0" encoding="UTF-8"?>
<one a="ack" b="back">
  <two/>
  <!-- just a comment --/>
  <three>my name is "ピーター"</three>
</one>
|, xml)
  end

  def test_builder_io
    IO.pipe do |r,w|
      if fork
        w.close
        xml = r.read(1000)
        xml.force_encoding('UTF-8')
        r.close
        assert_equal(%|<?xml version="1.0" encoding="UTF-8"?>
<one a="ack" b="back">
  <two/>
  <!-- just a comment --/>
  <three>my name is "ピーター"</three>
</one>
|, xml)
      else
        r.close
        b = Ox::Builder.io(w, :indent => 2)
        b.instruct(:xml, :version => '1.0', :encoding => 'UTF-8')
        b.element('one', :a => "ack", 'b' => 'back')
        b.element('two')
        b.pop()
        b.comment('just a comment')
        b.element('three')
        b.text("my name is \"ピーター\"")
        b.pop()
        b.pop()
        b.close()
        w.close
        Process.exit(0)
      end
    end
  end
  
  def test_builder_block
    xml = Ox::Builder.new(:indent => 2) { |b|
      b.instruct(:xml, :version => '1.0', :encoding => 'UTF-8')
      b.element('one', :a => "ack", 'b' => 'back') {
        b.element('two') {}
        b.comment('just a comment')
        b.element('three') {
          b.text("my name is \"ピーター\"")
        }
      }
    }
    assert_equal(%|<?xml version="1.0" encoding="UTF-8"?>
<one a="ack" b="back">
  <two/>
  <!-- just a comment --/>
  <three>my name is "ピーター"</three>
</one>
|, xml)
  end

  def test_builder_block_file
    filename = File.join(File.dirname(__FILE__), 'create_file_test.xml')
    Ox::Builder.file(filename, :indent => 2) { |b|
      b.instruct(:xml, :version => '1.0', :encoding => 'UTF-8')
      b.element('one', :a => "ack", 'b' => 'back') {
        b.element('two') {}
        b.comment('just a comment')
        b.element('three') {
          b.text("my name is \"ピーター\"")
        }
      }
    }    
    xml = File.read(filename)
    xml.force_encoding('UTF-8')
    assert_equal(%|<?xml version="1.0" encoding="UTF-8"?>
<one a="ack" b="back">
  <two/>
  <!-- just a comment --/>
  <three>my name is "ピーター"</three>
</one>
|, xml)
  end

  def test_builder_block_io
    IO.pipe do |r,w|
      if fork
        w.close
        xml = r.read(1000)
        xml.force_encoding('UTF-8')
        r.close
        assert_equal(%|<?xml version="1.0" encoding="UTF-8"?>
<one a="ack" b="back">
  <two/>
  <!-- just a comment --/>
  <three>my name is "ピーター"</three>
</one>
|, xml)
      else
        r.close
        Ox::Builder.io(w, :indent => 2) { |b|
          b.instruct(:xml, :version => '1.0', :encoding => 'UTF-8')
          b.element('one', :a => "ack", 'b' => 'back') {
            b.element('two') {}
            b.comment('just a comment')
            b.element('three') {
              b.text("my name is \"ピーター\"")
            }
          }
        }
        w.close
        Process.exit(0)
      end
    end
  end
  
  def test_builder_no_newline
    b = Ox::Builder.new(:indent => -1)
    b.instruct(:xml, :version => '1.0', :encoding => 'UTF-8')
    b.element('one', :a => "ack", 'b' => 'back')
    b.text('hello')
    b.close()
    xml = b.to_s
    assert_equal(%|<?xml version="1.0" encoding="UTF-8"?><one a="ack" b="back">hello</one>|, xml)
  end

  def test_builder_tolerant
    b = Ox::Builder.new(:effort => :tolerant)
    b.element('one')
    b.text("tab\tamp&backspace\b.")
    b.close()
    xml = b.to_s
    assert_equal(%|<one>tab\tamp&amp;backspace.</one>
|, xml)
  end

  def test_builder_strict
    begin
      b = Ox::Builder.new(:effort => :strict)
      b.element('one')
      b.text("tab\tamp&backspace\b.")
    rescue Exception => e
      assert_equal("'\\#x08' is not a valid XML character.", e.message)
      return
    end
    assert(false)
  end

  def dump_and_load(obj, trace=false, circular=false)
    xml = Ox.dump(obj, :indent => $indent, :circular => circular)
    puts xml if trace
    loaded = Ox.load(xml, :trace => (trace ? 2 : 0))
    if obj.nil?
      assert_nil(loaded)
    else
      assert_equal(obj, loaded)
    end
    loaded
  end

end

class Bag

  def initialize(args)
    args.each do |k,v|
      self.instance_variable_set(k, v)
    end
  end

  def eql?(other)
    return false if (other.nil? or self.class != other.class)
    ova = other.instance_variables
    return false if ova.size != instance_variables.size
    instance_variables.each do |vid|
      return false if instance_variable_get(vid) != other.instance_variable_get(vid)
    end
    true
  end
  alias == eql?

end
