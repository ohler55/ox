#!/usr/bin/env ruby -wW1

$: << '../lib'
$: << '../ext'

require 'optparse'
require 'stringio'
require 'ox'

$verbose = 0
$iter = 100

opts = OptionParser.new
opts.on('-v', 'increase verbosity')                            { $verbose += 1 }
opts.on('-i', '--iterations [Int]', Integer, 'iterations')     { |i| $iter = i }
opts.on('-h', '--help', 'Show this display')                   { puts opts; Process.exit!(0) }
files = opts.parse(ARGV)

### XML conversion to Hash using in memory Ox parsing ###

def node_to_dict(element)
  dict = Hash.new
  key = nil
  element.nodes.each do |n|
    raise 'A dict can only contain elements.' unless n.is_a?(Ox::Element)

    if key.nil?
      raise "Expected a key, not a #{n.name}." unless 'key' == n.name

      key = first_text(n)
    else
      dict[key] = node_to_value(n)
      key = nil
    end
  end
  dict
end

def node_to_array(element)
  a = Array.new
  element.nodes.each do |n|
    a.push(node_to_value(n))
  end
  a
end

def node_to_value(node)
  raise 'A dict can only contain elements.' unless node.is_a?(Ox::Element)

  case node.name
  when 'key'
    raise 'Expected a value, not a key.'
  when 'string'
    value = first_text(node)
  when 'dict'
    value = node_to_dict(node)
  when 'array'
    value = node_to_array(node)
  when 'integer'
    value = first_text(node).to_i
  when 'real'
    value = first_text(node).to_f
  when 'true'
    value = true
  when 'false'
    value = false
  else
    raise "#{node.name} is not a know element type."
  end
  value
end

def first_text(node)
  node.nodes.each do |n|
    return n if n.is_a?(String)
  end
  nil
end

def parse_gen(xml)
  doc = Ox.parse(xml)
  plist = doc.root
  dict = nil
  plist.nodes.each do |n|
    if n.is_a?(Ox::Element)
      dict = node_to_dict(n)
      break
    end
  end
  dict
end

### XML conversion to Hash using Ox SAX parser ###

class Handler
  def initialize()
    @key = nil
    @type = nil
    @plist = nil
    @stack = []
  end

  def text(value)
    last = @stack.last
    if last.is_a?(Hash) and @key.nil?
      raise "Expected a key, not #{@type} with a value of #{value}." unless :key == @type

      @key = value
    else
      append(value)
    end
  end

  def start_element(name)
    if :dict == name
      dict = Hash.new
      append(dict)
      @stack.push(dict)
    elsif :array == name
      a = Array.new
      append(a)
      @stack.push(a)
    elsif :true == name
      append(true)
    elsif :false == name
      append(false)
    else
      @type = name
    end
  end

  def end_element(name)
    @stack.pop if :dict == name or :array == name
  end
  
  attr_reader :plist

  def append(value)
    unless value.is_a?(Array) or value.is_a?(Hash)
      case @type
      when :string
        # ignore
      when :key
        # ignore
      when :integer
        value = value.to_i
      when :real
        value = value.to_f
      end
    end
    last = @stack.last
    if last.is_a?(Hash)
      raise "Expected a key, not with a value of #{value}." if @key.nil?

      last[@key] = value
      @key = nil
    elsif last.is_a?(Array)
      last.push(value)
    elsif last.nil?
      @plist = value
    end
  end
end

def parse_sax(xml)
  io = StringIO.new(xml)
  start = Time.now
  handler = Handler.new()
  Ox.sax_parse(handler, io)
  handler.plist
end

### XML conversion to Hash using Ox Object parsing with gsub! replacements ###

def convert_parse_obj(xml)
  xml = plist_to_obj_xml(xml)
  Ox.load(xml, :mode => :object)
end

### XML conversion to Hash using Ox Object parsing after gsub! replacements ###

def parse_obj(xml)
  Ox.load(xml, :mode => :object)
end

def plist_to_obj_xml(xml)
  xml = xml.gsub(%{<plist version="1.0">
}, '')
  xml.gsub!(%{
</plist>}, '')
  { '<dict>' => '<h>',
    '</dict>' => '</h>',
    '<dict/>' => '<h/>',
    '<array>' => '<a>',
    '</array>' => '</a>',
    '<array/>' => '<a/>',
    '<string>' => '<s>',
    '</string>' => '</s>',
    '<string/>' => '<s/>',
    '<key>' => '<s>',
    '</key>' => '</s>',
    '<integer>' => '<i>',
    '</integer>' => '</i>',
    '<integer/>' => '<i/>',
    '<real>' => '<f>',
    '</real>' => '</f>',
    '<real/>' => '<f/>',
    '<true/>' => '<y/>',
    '<false/>' => '<n/>',
  }.each do |pat, rep|
    xml.gsub!(pat, rep)
  end
  xml
end

files.each do |filename|
  xml = File.read(filename)

  if 0 < $verbose
    d1 = parse_gen(xml)
    d2 = parse_sax(xml)
    d3 = convert_parse_obj(xml)
    puts "--- It is #{d1 == d2 and d2 == d3} that all parsers yield the same Hash. ---"
  end

  start = Time.now
  $iter.times do
    parse_gen(xml)
  end
  gen_time = Time.now - start
  
  start = Time.now
  $iter.times do
    parse_sax(xml)
  end
  sax_time = Time.now - start

  start = Time.now
  $iter.times do
    convert_parse_obj(xml)
  end
  conv_obj_time = Time.now - start

  xml = plist_to_obj_xml(xml)
  start = Time.now
  $iter.times do
    parse_obj(xml)
  end
  obj_time = Time.now - start

  puts "In memory parsing and conversion took #{gen_time} for #{$iter} iterations."
  puts "SAX parsing and conversion took #{sax_time} for #{$iter} iterations."
  puts "XML gsub Object parsing and conversion took #{conv_obj_time} for #{$iter} iterations."
  puts "Object parsing and conversion took #{obj_time} for #{$iter} iterations."
end

# Results for a run:
#
# > parse_cmp.rb Sample.graffle -i 1000
# In memory parsing and conversion took 4.135701 for 1000 iterations.
# SAX parsing and conversion took 3.731695 for 1000 iterations.
# XML gsub Object parsing and conversion took 3.292397 for 1000 iterations.
# Object parsing and conversion took 0.808877 for 1000 iterations.
