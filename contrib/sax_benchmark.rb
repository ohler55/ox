# All credit to https://github.com/hakanensari
# Taken from https://gist.github.com/hakanensari/3078932

require 'benchmark'
require 'stringio'

require 'nokogiri'
require 'ox'

io = StringIO.new %{
<?xml version="1.0" encoding="UTF-8"?>
<ItemLookupResponse>
  <Items>
    <Item>
      <ASIN>0816614024</ASIN>
      <ItemAttributes>
        <Creator Role="Author">Gilles Deleuze</Creator>
        <Creator Role="Contributor">Felix Guattari</Creator>
        <Title>Thousand Plateaus</Title>
      </ItemAttributes>
    </Item>
    <Item>
      <ASIN>0231081596</ASIN>
      <ItemAttributes>
        <Creator Role="Author">Gilles Deleuze</Creator>
        <Title>Difference and Repetition</Title>
      </ItemAttributes>
    </Item>
  </Items>
</ItemLookupResponse>
}.strip.gsub(/>\s+</, '><')

class OxHandler < Ox::Sax
  attr :root

  def initialize
    super
    @stack = [@node = @root = {}]
  end

  def attr(key, val)
    @node[key] = val
  end

  def end_element(key)
    child = @stack.pop
    @node = @stack.last

    case @node[key]
    when Array
      @node[key] << child
    when Hash
      @node[key] = [@node[key], child]
    else
      if child.keys == [:__content__]
        @node[key] = child[:__content__]
      else
        @node[key] = child
      end
    end
  end

  def start_element(key)
    @stack << @node = {}
  end

  def text(val)
    @node[:__content__] = val
  end
end

class NokogiriHandler < Nokogiri::XML::SAX::Document
  attr :root

  def characters(val)
    (@node['__content__'] ||= '') << val
  end

  def end_element(key)
    child = @stack.pop
    @node = @stack.last

    case @node[key]
    when Array
      @node[key] << child
    when Hash
      @node[key] = [@node[key], child]
    else
      if child.keys == ['__content__']
        @node[key] = child['__content__']
      else
        @node[key] = child
      end
    end
  end

  def start_element(key, attrs = [])
    @stack << @node = {}
    attrs.each do |attr|
      key, val = *attr
      @node[key] = val
    end
  end

  def start_document
    @stack = [@root = {}]
  end
end

n = 10_000
Benchmark.bmbm do |b|
  b.report('ox') do
    n.times do
      io.rewind
      handler = OxHandler.new
      Ox.sax_parse handler, io
    end
  end

  b.report('nokogiri') do
    n.times do
      io.rewind
      handler = NokogiriHandler.new
      parser = Nokogiri::XML::SAX::Parser.new handler
      parser.parse io
    end
  end
end
