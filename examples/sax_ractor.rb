#!/usr/bin/env ruby

require 'ox'
require 'pathname'

# Silence Ractor warning in Ruby 3.0.x
Warning[:experimental] = false
abort('This Ractor example requires at least Ruby 3.0') if RUBY_VERSION.start_with?('2')

# Example/test script for `Ractor`-based `Ox::Sax` parsing.
# In the Real World™ we probably wouldn't create a single-use `Ractor` for
# every argument, but this is primarily a test of `rb_ext_ractor_safe` for Ox.


# Miniature example Ractor-based `shared-mime-info` Ox handler à la `CHECKING::YOU::OUT`:
# https://github.com/okeeblow/DistorteD/tree/NEW-SENSATION/CHECKING-YOU-OUT
class Saxtor < Ox::Sax

  # We will fill this `Struct` as we parse,
  # yield it if its `ietf` matches our `needle`,
  # and throw it away otherwise.
  CYO = Struct.new(:ietf, :globs, :description) do
    def initialize(ietf = nil, globs = [], description = nil)
      super(ietf, globs, description)
    end
    def to_s  # Pretty print
      "#{self[:description]} (#{self[:ietf]}) [#{
        self[:globs]&.map(&File.method(:extname)).join(',')
      }]"
    end
    def inspect
      "#<CYO #{to_s}>"
    end
  end

  # Set up our parsing environment and open a file handle for our XML.
  def initialize(parent, haystack)
    @parse_stack = []  # Track our current Element as we parse.
    @parent = parent           # `Ractor` that instantiated us.
    @haystack = File.open(haystack, File::Constants::RDONLY)
    @haystack.advise(:sequential)
  end

  # Stratch `Struct`.
  def cyo
    @cyo ||= CYO.new
  end

  # Wax on…
  def start_element(name)
    @parse_stack.push(name)
    case @parse_stack.last
    when :"mime-type" then @cyo = nil  # Clear out leftovers between types.
    end
  end

  # …wax off.
  def end_element(name)
    case @parse_stack.last
    when :"mime-type"
      # Save the scratch `Struct` if we matched our needle while building it.
      @out = cyo.dup if @i_can_haz
      @i_can_haz = false
    end
    @parse_stack.pop
  end

  # Element attribute callback — Ox::Sax::Value version
  def attr_value(name, value)
    case [@parse_stack.last, name]
    in :"mime-type", :type
      cyo[:ietf] = value.as_s
      # If we found our needle then we will yield the scratch `CYO` instead of `nil`.
      @i_can_haz = true if value.as_s == @needle
    in :glob, :pattern
      cyo[:globs].append(value.as_s)
    else nil
    end
  end

  # Element text content callback, e.g. for <example>TEXT</example>
  #                                This part. --------^
  def text(element_text)
    case @parse_stack.last
    when :comment
      # This will end up being the `last` <comment> locale (probably `zh_TW`)
      # because I don't want to implement locale checking for a test script lol
      cyo[:description] = element_text
    end
  end

  # Start our search for a given `needle` in our open `haystack`.
  def awen(needle, **kwargs)
    @needle = needle    # What IETF Media-Type should we find?  (e.g. `'image/jpeg'`)
    @i_can_haz = false  # Did we find our `needle`? (obviously not yet)
    @haystack.rewind    # Pon de Replay

    # Do the thing.
    Ox.sax_parse(
      self,                     # Instance of a class that responds to `Ox::Sax`'s callback messages.
      @haystack,                # IO stream or String of XML to parse. Won't close File handles automatically.
      **{
        convert_special: true,  # [boolean] Convert encoded entities back to their unencoded form, e.g. `"&lt"` to `"<"`.
        skip: :skip_off,        # [:skip_none|:skip_return|:skip_white|:skip_off] (from Element text/value) Strip CRs, whitespace, or nothing.
        smart: false,           # [boolean] Toggle Ox's built-in hints for HTML parsing: https://github.com/ohler55/ox/blob/master/ext/ox/sax_hint.c
        strip_namespace: true,  # [nil|String|true|false] (from Element names) Strip no namespaces, all namespaces, or a specific namespace.
        symbolize: true,        # [boolean] Fill callback method `name` arguments with Symbols instead of with Strings.
        intern_string_values: true,   # [boolean] Intern (freeze and deduplicate) String return values.
      }.update(kwargs),
    )

    # Let our parent `#take` our needle-equivalent `CYO`, or `nil`.
    Ractor.yield(@out)
  end  # def awen

end  # class Saxtor

# Fancy "usage" help `String` fragment to concat with specific error messages.
usage = <<-PLZ

Usage: `sax_ractor.rb [SHARED-MIME-INFO_XML_PATH] [IETF_MEDIA_TYPES]…`

Common file paths:

- FreeBSD:
  `${LOCALBASE}/share/mime/packages/freedesktop.org.xml` (probably `/usr/local`)
  https://www.freshports.org/misc/shared-mime-info/

- Linux:
  `/usr/share/mime/packages/freedesktop.org.xml`

- macOS:
  `/opt/homebrew/share/mime/packages/freedesktop.org.xml` (Homebrew)
  `/opt/local/share/mime/packages/freedesktop.org.xml`    (MacPorts)
  https://formulae.brew.sh/formula/shared-mime-info
PLZ

# Bail out if we were given a nonexistant file.
unless ARGV.size > 0
  abort("Please provide the path to a `shared-mime-info` XML package \
  and some media-type query arguments (e.g. 'image/jpeg')".concat(usage))
end
haystack = Pathname.new(ARGV.first)
abort("#{haystack} does not exist") unless haystack.exist? and haystack.file?

# *Judicator Aldaris voice* "YOU HAVE NOT ENOUGH ARGUMENTS."
abort("Please provide some media-type query arguments (e.g. 'image/jpeg')".concat(usage)) unless ARGV.size > 1

# We can use `Ractor::make_shareable()` for larger traversable data structures,
# but freezing should be enough to share a `Pathname`.
# Resolve symlinks etc with `#realpath` before we freeze.
haystack = haystack.realpath.freeze
needles = ARGV[1...]


# Hamburger Style.
puts 'Parallel Ractors'
# Create one `Ractor` for every given media-type argument
moo = ['Heifer', 'Cow', 'Bull', 'Steer'].tally
head_count = needles.size - 1
herd = (0..head_count).map {
  # Give our worker `Ractor` a name, otherwise its `#name` will return `nil`.
  individual = moo.keys.sample
  moo[individual] += 1
  Ractor.new(haystack, name: "#{individual} #{moo[individual] - 1}") { |haystack|
    # Initialize an `Ox::Sax` handler for our given source file.
    handler = Saxtor.new(Ractor.current, haystack)

    # Now we can `#send` a needle to this `Ractor` and make it search the haystack!
    while ietf_string = Ractor.receive
      Ractor.yield(handler.awen(ietf_string))
    end
  }
}

# Send our arguments to our herd in a 1:1 mapping
(0..head_count).each { herd[_1].send(needles[_1]) }

# Wait for every `Ractor` to have a result, and then pretty print all of them :)
pp (0..head_count).map {
  [herd[_1], herd[_1].take]
}.map {
  "#{_1.name} gave us #{_2 || 'nothing'}"
}


# Hotdog Style.
puts
puts 'Serial Ractor'
# Create a single `Ractor` and send every media-type to it in series.
only_one_ox = Ractor.new(haystack, name: 'ONLY ONE OX') { |haystack|
  handler = Saxtor.new(Ractor.current, haystack)
  while ietf_string = Ractor.receive
    handler.awen(ietf_string)
  end
}
(0..head_count).each { only_one_ox.send(needles[_1]) }
pp "#{only_one_ox.name} gave us #{(0..head_count).map { only_one_ox.take }}"
