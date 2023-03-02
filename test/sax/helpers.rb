require 'handlers'

module SaxTestHelpers
  # A helper method to initiate a sax parsing using a specified xml
  # structure as input, an expected stack of calls, a handler class
  # and an optional options hash to pass to the parser.
  #
  # The options that the parser recognizes are :convert_special and
  # :smart, which have both boolean values.
  #
  # E.g.
  # parse_compare(%{<top> This is some text.</top>},
  #                [[:start_element, :top],
  #                 [:text, " This is some text."],
  #                 [:end_element, :top]
  #                ], AllSax, :convert_special => true, :smart => true)
  #
  def parse_compare(xml, expected, handler_class = AllSax, opts = {}, handler_attr = :calls)
    handler = handler_class.new
    input = StringIO.new(xml)
    options = {
      :symbolize => true,
      # :convert_special => true,
      :smart => false
    }.merge(opts)

    Ox.sax_parse(handler, input, options)

    actual = handler.send(handler_attr)

    if expected != actual
      expected.each_index do |i|
        puts "#{i}: #{expected[i]} != #{actual[i]}" if expected[i] != actual[i]
      end
    end
    puts "\nexpected: #{expected}\n  actual: #{actual}" if expected != actual
    assert_equal(expected, actual)
  end

  # This is needed to stop test/unit from complaining that there is no test
  # specified.
  def test_hack
    assert(true)
  end
end
