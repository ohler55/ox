#!/usr/bin/env ruby -wW1

$: << '.'
$: << '..'
$: << '../lib'
$: << '../ext'

if __FILE__ == $0
  while (i = ARGV.index('-I'))
    x,path = ARGV.slice!(i, 2)
    $: << path
  end
end

require 'optparse'
require 'pp'
require 'stringio'
require 'ox'

$verbose = 0
$iter = 100

opts = OptionParser.new
opts.on("-v", "increase verbosity")                            { $verbose += 1 }
opts.on("-i", "--iterations [Int]", Integer, "iterations")     { |i| $iter = i }
opts.on("-h", "--help", "Show this display")                   { puts opts; Process.exit!(0) }
files = opts.parse(ARGV)

### XML conversion to Hash using in memory Ox parsing ###

def node_to_dict(node)
  dict = Hash.new
  key = nil
  node.each do |n|
    if n.is_a?(::Ox::Element)
      # if name is key...
      # if dict
      # TBD
    elsif n.is_a?(String)
      # TBD
    end
  end
  return dict
end

def parse_gen(xml)
  doc = Ox.parse(xml)
  plist = doc.root
  dict = nil
  plist.nodes.each do |n|
    if n.is_a?(::Ox::Element)
      dict = node_to_dict(n)
      break
    end
  end
  pp dict
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
  
  def plist
    @plist
  end

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
  begin 
    Ox.sax_parse(handler, io)
  rescue Exception => e
    puts "#{e.class}: #{e.message}"
    e.backtrace.each { |line| puts line }
  end
end

### XML conversion to Hash using Ox Object parsing with gsub! replacements ###

def convert_parse_obj(xml)
  xml = plist_to_obj_xml(xml)
  ::Ox.load(xml, :mode => :object)
end

### XML conversion to Hash using Ox Object parsing after gsub! replacements ###

def parse_obj(xml)
  ::Ox.load(xml, :mode => :object)
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
  }.each do |pat,rep|
    xml.gsub!(pat, rep)
  end
  xml
end

files.each do |filename|
  xml = File.read(filename)

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
