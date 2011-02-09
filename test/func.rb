#!/usr/bin/env ruby -wW1

$: << '../lib'
$: << '../ext'

if __FILE__ == $0
  if (i = ARGV.index('-I'))
    x,path = ARGV.slice!(i, 2)
    $: << path
  end
end

require 'test/unit'
require 'optparse'
require 'ox'

$indent = 2

opts = OptionParser.new
# TBD add indent
opts.on("-h", "--help", "Show this display")                { puts opts; Process.exit!(0) }
files = opts.parse(ARGV)

class Func < ::Test::Unit::TestCase
  
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
    dump_and_load((0..3), false)
    dump_and_load((-2..3.7), false)
    dump_and_load(('a'...'f'), false)
  end

  def test_regex
    dump_and_load(/^[0-9]/ix, false)
    dump_and_load(/^[&0-9]/ix, false) # with xml-unfriendly character
  end

  def test_bignum
    dump_and_load(7 ** 55, false)
    dump_and_load(-7 ** 55, false)
  end

  def test_complex_number
    dump_and_load(Complex(1), false)
    dump_and_load(Complex(3, 2), false)
  end

  def test_rational
    dump_and_load(Rational(1, 3), false)
    dump_and_load(Rational(0, 3), false)
  end

  def test_object
    dump_and_load(Bag.new({ }), false)
    dump_and_load(Bag.new(:@x => 3), false)
  end
  
  def test_class
    dump_and_load(Bag, false)
  end

  def test_exception
    e = Exception.new("Some Error")
    e.set_backtrace(["./func.rb:119: in test_exception",
                     "./fake.rb:57: in fake_func"])
    dump_and_load(e, true)
  end

  def test_struct
    s = Struct.new('Box', :x, :y, :w, :h)
    dump_and_load(s.new(2, 4, 10, 20), false)
  end

  def test_array_multi
    dump_and_load([nil, true, false, 3, 'z', 7.9, 'a&b', :xyz, Time.now, (-1..7)], false)
  end

  def test_hash_multi
    dump_and_load({ nil => nil, true => true, false => false, 3 => 3, 'z' => 'z', 7.9 => 7.9, 'a&b' => 'a&b', :xyz => :xyz, Time.now => Time.now, (-1..7) => (-1..7) }, false)
  end

  def test_object_multi
    dump_and_load(Bag.new(:@a => nil, :@b => true, :@c => false, :@d => 3, :@e => 'z', :@f => 7.9, :@g => 'a&b', :@h => :xyz, :@i => Time.now, :@j => (-1..7)), false)
  end

  def test_complex
    dump_and_load(Bag.new(:@o => Bag.new(:@a => [2]), :@a => [1, {b: 3, a: [5]}, c: Bag.new(:@x => 7)]), false)
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

  def dump_and_load(obj, trace=false)
    xml = Ox.dump(obj, :indent => $indent)
    puts xml if trace
    loaded = Ox.load(xml, :mode => :object, :trace => (trace ? 2 : 0))
    assert_equal(obj, loaded)
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
