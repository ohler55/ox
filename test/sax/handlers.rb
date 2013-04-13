require 'ox'


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

  def end_instruct(target)
    @calls << [:end_instruct, target]
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


class LineColSax < StartSax
  def initialize()
    @line = nil   # this initializes the @line variable which will then be set by the parser
    @column = nil # this initializes the @line variable which will then be set by the parser
    super
  end

  def instruct(target)
    @calls << [:instruct, target, @line, @column]
  end

  def start_element(name)
    @calls << [:start_element, name, @line, @column]
  end

  def end_instruct(target)
    @calls << [:end_instruct, target, @line, @column]
  end

  def doctype(value)
    @calls << [:doctype, value, @line, @column]
  end

  def comment(value)
    @calls << [:comment, value, @line, @column]
  end

  def cdata(value)
    @calls << [:cdata, value, @line, @column]
  end

  def text(value)
    @calls << [:text, value, @line, @column]
  end

  def end_element(name)
    @calls << [:end_element, name, @line, @column]
  end

  def attr(name, value)
    @calls << [:attr, name, value, @line, @column]
  end

  def error(message, line, column)
    @calls << [:error, message, line, column]
  end
end


class TypeSax < ::Ox::Sax
  attr_accessor :item
  # method to call on the Ox::Sax::Value Object
  attr_accessor :type

  def initialize(type)
    @item = nil
    @type = type
  end

  def attr_value(name, value)
    @item = value.send(name)
  end

  def value(value)
    @item = value.send(@type)
  end
end


class ErrorSax < ::Ox::Sax
  attr_reader :errors

  def initialize
    @path = []
    @tags = []
    @errors = []
  end

  def start_element(tag)
    @tags << tag
    @path & @tags
  end

  def error(message, line, column)
    @errors << message
  end
end

class HtmlSax < ::Ox::Sax
  attr_accessor :calls

  def initialize()
    @calls = []
  end

  def start_element(name)
    @calls << [:start_element, name]
  end

  def end_element(name)
    @calls << [:end_element, name]
  end

  def attr(name, value)
    @calls << [:attr, name, value]
  end

  def instruct(target)
    @calls << [:instruct, target]
  end

  def end_instruct(target)
    @calls << [:end_instruct, target]
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

  def error(message, line, column)
    @calls << [:error, message, line, column]
  end
end
