#!/usr/bin/env ruby -wW1
# encoding: UTF-8

$: << File.join(File.dirname(__FILE__), "../lib")
$: << File.join(File.dirname(__FILE__), "../ext")

require 'test/unit'
require 'optparse'
require 'ox'

$indent = 2

opts = OptionParser.new
# TBD add indent
opts.on("-h", "--help", "Show this display")                { puts opts; Process.exit!(0) }
files = opts.parse(ARGV)

class Func < ::Test::Unit::TestCase

  def test_get_options
    opts = Ox.default_options()
    assert_equal(opts, {
                   :encoding=>nil,
                   :indent=>2,
                   :trace=>0,
                   :with_dtd=>false,
                   :with_xml=>false,
                   :with_instructions=>false,
                   :circular=>false,
                   :xsd_date=>false,
                   :mode=>nil,
                   :effort=>:strict})
  end

  def test_set_options
    orig = {
      :encoding=>nil,
      :indent=>2,
      :trace=>0,
      :with_dtd=>false,
      :with_xml=>true,
      :with_instructions=>false,
      :circular=>false,
      :xsd_date=>false,
      :mode=>nil,
      :effort=>:strict}
    o2 = {
      :encoding=>"UTF-8",
      :indent=>4,
      :trace=>1,
      :with_dtd=>true,
      :with_xml=>false,
      :with_instructions=>true,
      :circular=>true,
      :xsd_date=>true,
      :mode=>:object,
      :effort=>:tolerant }
    o3 = { :xsd_date=>false }
    Ox.default_options = o2
    opts = Ox.default_options()
    assert_equal(opts, o2);
    Ox.default_options = o3 # see if it throws an exception
    Ox.default_options = orig # return to original
  end

  def test_nil
    dump_and_load(nil, false)
  end

  def test_true
    dump_and_load(true, false)
  end

  def test_false
    dump_and_load(false, false)
  end

  def test_fixnum
    dump_and_load(7, false)
    dump_and_load(-19, false)
    dump_and_load(0, false)
  end

  def test_float
    dump_and_load(7.7, false)
    dump_and_load(-1.9, false)
    dump_and_load(0.0, false)
    dump_and_load(-10.000005, false)
    dump_and_load(1.000000000005, false)
  end

  def test_string
    dump_and_load('a string', false)
    dump_and_load('', false)
  end

  def test_symbol
    dump_and_load(:a_symbol, false)
    dump_and_load(:<=, false)
  end

  def test_base64
    dump_and_load('a & x', false)
  end

  def test_time
    dump_and_load(Time.now, false)
  end

  def test_array
    dump_and_load([], false)
    dump_and_load([1, 'a'], false)
  end

  def test_hash
    dump_and_load({ }, false)
    dump_and_load({ 'a' => 1, 2 => 'b' }, false)
  end

  def test_range
    if RUBY_VERSION.start_with?('1.8')
      assert(true)
    else
      dump_and_load((0..3), false)
      dump_and_load((-2..3.7), false)
      dump_and_load(('a'...'f'), false)
      t = Time.now
      t2 = t + 20
      dump_and_load((t..t2), false)
    end
  end

  def test_regex
    if RUBY_VERSION.start_with?('1.8')
      assert(true)
    else
      dump_and_load(/^[0-9]/ix, false)
      dump_and_load(/^[&0-9]/ix, false) # with xml-unfriendly character
    end
  end

  def test_bignum
    dump_and_load(7 ** 55, false)
    dump_and_load(-7 ** 55, false)
  end

  def test_complex_number
    if RUBY_VERSION.start_with?('1.8')
      assert(true)
    else
      dump_and_load(Complex(1), false)
      dump_and_load(Complex(3, 2), false)
    end
  end

  def test_rational
    if RUBY_VERSION.start_with?('1.8')
      assert(true)
    else
      dump_and_load(Rational(1, 3), false)
      dump_and_load(Rational(0, 3), false)
    end
  end

  def test_object
    dump_and_load(Bag.new({ }), false)
    dump_and_load(Bag.new(:@x => 3), false)
  end
  
  def test_bad_object
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
    assert_equal(loaded, nil)
    loaded = Ox.load(xml, :mode => :object, :trace => 0, :effort => :auto_define)
    assert_equal(loaded.class.to_s, 'Bad::Boy')
    assert_equal(loaded.class.superclass.to_s, 'Ox::Bag')
    loaded = Ox.load(xml2, :mode => :object, :trace => 0, :effort => :auto_define)
    assert_equal(loaded.class.to_s, 'Bad')
    assert_equal(loaded.class.superclass.to_s, 'Ox::Bag')
  end

  def test_xml_instruction
    xml = Ox.dump("test", :mode => :object, :with_xml => false)
    assert_equal("<s>test</s>\n", xml)
    xml = Ox.dump("test", :mode => :object, :with_xml => true)
    assert_equal("<?xml version=\"1.0\"?>\n<s>test</s>\n", xml)
    xml = Ox.dump("test", :mode => :object, :with_xml => true, :encoding => 'UTF-8')
    assert_equal("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<s>test</s>\n", xml)
  end

  def test_ox_instruction
    xml = Ox.dump("test", :mode => :object, :with_xml => true, :with_instructions => true)
    assert_equal("<?xml version=\"1.0\"?>\n<?ox version=\"1.0\" mode=\"object\" circular=\"no\" xsd_date=\"no\"?>\n<s>test</s>\n", xml)
    xml = Ox.dump("test", :mode => :object, :with_instructions => true)
    assert_equal("<?ox version=\"1.0\" mode=\"object\" circular=\"no\" xsd_date=\"no\"?>\n<s>test</s>\n", xml)
    xml = Ox.dump("test", :mode => :object, :with_instructions => true, :circular => true, :xsd_date => true)
    assert_equal("<?ox version=\"1.0\" mode=\"object\" circular=\"yes\" xsd_date=\"yes\"?>\n<s i=\"1\">test</s>\n", xml)
    xml = Ox.dump("test", :mode => :object, :with_instructions => true, :circular => false, :xsd_date => false)
    assert_equal("<?ox version=\"1.0\" mode=\"object\" circular=\"no\" xsd_date=\"no\"?>\n<s>test</s>\n", xml)
  end

  def test_dtd
    xml = Ox.dump("test", :mode => :object, :with_dtd => true)
    assert_equal("<!DOCTYPE s SYSTEM \"ox.dtd\">\n<s>test</s>\n", xml)
  end

  def test_lone_dtd
    xml = "<!DOCTYPE html>" # not really a valid xml but should pass anyway
    doc = Ox.parse(xml)
    assert_equal('html', doc.nodes[0].value)
  end

  def test_quote_value
    xml = %{<top name="Pete"/>}
    doc = Ox.parse(xml)
    assert_equal('Pete', doc.attributes[:name])
  end

  def test_single_quote
    xml = %{<top name='Pete'/>}
    doc = Ox.load(xml, :effort => :tolerant)
    assert_equal('Pete', doc.attributes[:name])
  end

  def test_no_quote
    xml = %{<top name=Pete />}
    doc = Ox.load(xml, :effort => :tolerant)
    assert_equal('Pete', doc.attributes[:name])
  end

  def test_class
    dump_and_load(Bag, false)
  end

  def test_exception
    if RUBY_VERSION.start_with?('1.8')
      assert(true)
    else
      e = StandardError.new("Some Error")
      e.set_backtrace(["./func.rb:119: in test_exception",
                       "./fake.rb:57: in fake_func"])
    end
    dump_and_load(e, false)
  end

  def test_struct
    s = Struct.new('Box', :x, :y, :w, :h)
    dump_and_load(s.new(2, 4, 10, 20), false)
  end

  def test_bad_format
    xml = "<?xml version=\"1.0\"?>\n<tag>test</tagz>\n"
    assert_raise(SyntaxError) {
      Ox.load(xml, :mode => :generic, :trace => 0)
    }
  end

  def test_array_multi
    if RUBY_VERSION.start_with?('1.8')
      dump_and_load([nil, true, false, 3, 'z', 7.9, 'a&b', :xyz, Time.now], false)
    else
      dump_and_load([nil, true, false, 3, 'z', 7.9, 'a&b', :xyz, Time.now, (-1..7)], false)
    end
  end

  def test_hash_multi
    if RUBY_VERSION.start_with?('1.8')
      dump_and_load({ nil => nil, true => true, false => false, 3 => 3, 'z' => 'z', 7.9 => 7.9, 'a&b' => 'a&b', :xyz => :xyz, Time.now => Time.now }, false)
    else
      dump_and_load({ nil => nil, true => true, false => false, 3 => 3, 'z' => 'z', 7.9 => 7.9, 'a&b' => 'a&b', :xyz => :xyz, Time.now => Time.now, (-1..7) => (-1..7) }, false)
    end
  end

  def test_object_multi
    if RUBY_VERSION.start_with?('1.8')
      dump_and_load(Bag.new(:@a => nil, :@b => true, :@c => false, :@d => 3, :@e => 'z', :@f => 7.9, :@g => 'a&b', :@h => :xyz, :@i => Time.now), false)
    else
      dump_and_load(Bag.new(:@a => nil, :@b => true, :@c => false, :@d => 3, :@e => 'z', :@f => 7.9, :@g => 'a&b', :@h => :xyz, :@i => Time.now, :@j => (-1..7)), false)
    end
  end

  def test_complex
    dump_and_load(Bag.new(:@o => Bag.new(:@a => [2]), :@a => [1, {:b => 3, :a => [5], :c => Bag.new(:@x => 7)}]), false)
  end

  # Create an Object and an Array with the same Objects in them. Dump and load
  # and then change the ones in the loaded Object to verify that the ones in
  # the array change in the same way. They are the same objects so they should
  # change. Perform the operation on both the object before and the loaded so
  # the equal() method can be used.
  def test_circular
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

  def test_raw
    raw = Ox::Element.new('raw')
    su = Ox::Element.new('sushi')
    su[:kind] = 'fish'
    raw << su
    ba = Ox::Element.new('basashi')
    ba[:kind] = 'animal'
    raw << ba
    dump_and_load(Bag.new(:@raw => raw), false)
  end

  def dump_and_load(obj, trace=false, circular=false)
    xml = Ox.dump(obj, :indent => $indent, :circular => circular)
    puts xml if trace
    loaded = Ox.load(xml, :mode => :object, :trace => (trace ? 2 : 0))
    assert_equal(obj, loaded)
    loaded
  end

  def test_nameerror
    begin
      "x".foo
    rescue Exception => e
      xml = Ox.dump(e, :effort => :tolerant)
      o = Ox.load(xml, :mode => :object)
      xml2 = Ox.dump(o, :effort => :tolerant)
      assert_equal(xml, xml2)
    end
  end
  
  def test_mutex
    if defined?(Mutex)
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
    if RUBY_VERSION.start_with?('1.8')
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
    if RUBY_VERSION.start_with?('1.8')
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
    if RUBY_VERSION.start_with?('1.8')
      assert(true)
    else
      orig = Ox.default_options
      opts = orig.clone
      opts[:encoding] = 'UTF-8'
      opts[:with_xml] = true
      Ox.default_options = opts
      begin
        dump_and_load(Bag.new(:@tsuma => :まきえ), false)
        dump_and_load(Bag.new(:@つま => :まきえ), false)
      ensure
        Ox.default_options = orig
      end
    end
  end

  def test_instructions
    xml = Ox.dump("test", :with_instructions => true)
    #puts xml
    obj = Ox.load(xml) # should convert it to an object
    assert_equal("test", obj)
  end

  def test_IO
    f = File.open(__FILE__, "r")
    assert_raise(NotImplementedError) {
      xml = Ox.dump(f, :effort => :strict)
    }
    xml = Ox.dump(f, :effort => :tolerant)
    obj = Ox.load(xml, :mode => :object) # should convert it to an object
    assert_equal(nil, obj)
  end
  
  def locate_xml()
    %{<?xml?>
<Family real="false">
  <Pete age="57" type="male">
    <Kid1 age="32"/>
    <Kid2 age="31"/>
  </Pete>
</Family>
}
  end
  
  def test_locate_self
    doc = Ox.parse(locate_xml)
    nodes = doc.locate(nil)
    assert_equal(doc, nodes[0])
  end

  def test_locate_top
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('Family')
    assert_equal([doc.nodes[0]], nodes)
  end

  def test_locate_top_wild
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('?')
    assert_equal([doc.nodes[0]], nodes)
  end

  def test_locate_child
    doc = Ox.parse(locate_xml)

    nodes = doc.locate('Family/?')
    assert_equal(['Pete'], nodes.map { |n| n.name })

    nodes = doc.locate('Family/?/?')
    assert_equal(['Kid1', 'Kid2'], nodes.map { |n| n.name })

    nodes = doc.locate('Family/Pete/?')
    assert_equal(['Kid1', 'Kid2'], nodes.map { |n| n.name })

    nodes = doc.locate('Family/Makie/?')
    assert_equal([], nodes.map { |n| n.name })
  end

  def test_locate_child_wild_wild
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('Family/?/?')
    assert_equal(['Kid1', 'Kid2'], nodes.map { |n| n.name })
  end

  def test_locate_child_wild
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('Family/Pete/?')
    assert_equal(['Kid1', 'Kid2'], nodes.map { |n| n.name })
  end

  def test_locate_child_wild_missing
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('Family/Makie/?')
    assert_equal([], nodes.map { |n| n.name })
  end

  def test_locate_attribute
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

    assert_raise(::Ox::InvalidPath) {
      nodes = doc.locate('Family/@age/?')
    }

    assert_raise(::Ox::InvalidPath) {
      nodes = doc.locate('Family/?[/?')
    }
  end

  def test_locate_qual_index
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('Family/Pete/?[0]')
    assert_equal(['Kid1'], nodes.map { |e| e.name } )
    nodes = doc.locate('Family/Pete/?[1]')
    assert_equal(['Kid2'], nodes.map { |e| e.name } )
  end

  def test_locate_qual_less
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('Family/Pete/?[<1]')
    assert_equal(['Kid1'], nodes.map { |e| e.name } )
  end

  def test_locate_qual_great
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('Family/Pete/?[>0]')
    assert_equal(['Kid2'], nodes.map { |e| e.name } )
  end

  def test_locate_qual_last
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('Family/Pete/?[-1]')
    assert_equal(['Kid2'], nodes.map { |e| e.name } )
  end

  def test_locate_qual_last_attr
    doc = Ox.parse(locate_xml)
    nodes = doc.locate('Family/Pete/?[-1]/@age')
    assert_equal(['31'], nodes )
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
