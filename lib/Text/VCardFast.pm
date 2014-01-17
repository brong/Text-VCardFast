package Text::VCardFast;

use 5.014002;
use strict;
use warnings;

use Encode qw(decode_utf8 encode_utf8);
use MIME::Base64 qw(decode_base64 encode_base64);

require Exporter;

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use Text::VCardFast ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
	vcard2hash
	hash2vcard
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
	vcard2hash
	hash2vcard
);

our $VERSION = '0.02';

require XSLoader;
XSLoader::load('Text::VCardFast', $VERSION);

# public API

sub vcard2hash { &vcard2hash_c }
sub hash2vcard { &hash2vcard_pp }

# Implementation

sub vcard2hash_c {
    my $vcard = shift;
    my %params = @_;
    if (utf8::is_utf8($vcard)) {
        utf8::encode($vcard);
        $params{is_utf8} = 1;
    }
    my $hash = Text::VCardFast::_vcard2hash($vcard, \%params);
    return $hash;
}

# pureperl version

# VCard parsing and formatting {{{

my %RFC6868Map = ("n" => "\n", "^" => "^", "'" => "\"");
my %RFC6868RevMap = reverse %RFC6868Map;
my %UnescapeMap = ("n" => "\n", "N" => "\n");

my $Pos = 1;
my @PropOutputOrder = qw(version fn n nickname lang gender org title role bday anniversary email tel adr url impp);
my %PropOutputOrder = map { $_ => $Pos++ } @PropOutputOrder;
my @ParamOutputOrder = qw(type pref);
my %ParamOutputOrder = map { $_ => $Pos++ } @ParamOutputOrder;

sub vcard2hash_pp {
  my $vcard = shift;
  my %params = @_;
  return vcardlines2hash_pp(\%params, (split /\r?\n/, $vcard));
}

sub vcardlines2hash_pp {
  my $params = shift;
  local $_;

  my %MultiFieldMap;
  if ($params->{multival}) {
    %MultiFieldMap = map { $_ => 1 } @{$params->{multival}};
  }

  # rfc2425, rfc2426, rfc6350, rfc6868

  my @Path;
  my $Current;
  while ($_ = shift @_) {
    # Strip EOL
    s/\r?\n$//;

    # 5.8.1 - Unfold lines if next line starts with space or tab
    if (@_ && $_[0] =~ s/^[ \t]//) {
      $_ .= shift @_;
      redo;
    }

    # Ignore empty lines
    next if /^\s*$/;

    if (/^BEGIN:(.*)/i) {
      push @Path, $Current;
      $Current = { type => uc $1 };
      push @{ $Path[-1]{objects} }, $Current;
      next;
    }
    if (/^END:(.*)/i) {
      die "END $1 in $Current->{type}"
        unless $Current->{type} eq uc $1;
      $Current = pop @Path;
      next;
    }

    # 5.8.2 - Parse '[group "."] name *(";" param) ":" value'
    #  In v2.1, params may not have "=value" part
    #  In v4, "," is allowed in non-quoted param value
    my ($Name) = /^([^;:]*)/gc;
    my @Params = /\G;(?:([\w\-]+)=)?("[^"]*"|[^";:=]*)/gc;
    my ($Value) = /\G:(.*)$/g;

    # 5.8.2 - Type names and parameter names are case insensitive
    my $LName = lc $Name;

    my %Props;

    # Remove group from each property name and add as attribute
    #  (in v4, group names are case insensitive as well)
    if ($LName =~ s/^(.+)\.(.*?)$/$2/) {
      $Props{group} = $1;
    }

    $Props{name} = $LName;

    # Parse out parameters
    my $Params = ($Props{params} //= {});
    while (@Params) {
      # Parsed into param => param-value pairs
      my ($PName, $PValue) = splice @Params, 0, 2;
      $PName = 'type' if !defined $PName;

      # 5.8.2 - parameter names are case insensitive
      my $LPName = lc $PName;

      $PValue =~ s/^"(.*)"$/$1/;
      # \n needed for label, but assume any \; is meant to be ; as well
      $PValue =~ s#\\(.)#$UnescapeMap{$1} // $1#ge;
      # And RFC6868 recoding
      $PValue =~ s/\^([n^'])/$RFC6868Map{$1}/g;

      if (!exists $Params->{$LPName}) {
        $Params->{$LPName} = $PValue;
      } elsif (!ref $Params->{$LPName}) {
        $Params->{$LPName} = [ $Params->{$LPName}, $PValue ];
      } else {
        push @{$Params->{$LPName}}, $PValue;
      }
    }
    delete $Props{params} unless keys %{$Props{params}};

    my $Encoding = $Params->{encoding};

    if ($MultiFieldMap{$LName}) {
      # use negative 'limit' to force trailing fields
      $Value = [ split /(?<!\\);/, $Value, -1 ];
      s#\\(.)#$UnescapeMap{$1} // $1#ge for @$Value;
      $Props{values} = $Value;
    } elsif ($Encoding && lc $Encoding->[0] eq 'b') {
      # Don't bother unescaping base64 value

      $Props{value} = $Value;
    } else {
      $Value =~ s#\\(.)#$UnescapeMap{$1} // $1#ge;
      $Props{value} = $Value;
    }

    push @{$Current->{properties}->{$LName}}, \%Props;
  }

  # something did a BEGIN but no END - TODO, unwind this nicely as
  # it may be more than one level
  die "BEGIN $Current->{type} without matching END"
    if @Path;

  return $Current;
}

sub hash2vcard_pp {
  return join "", map { $_ . ($_[1] // "\n") } hash2vcardlines_pp($_[0]);
}

sub hash2vcardlines_pp {
  my $Objects = shift->{objects};

  my @Lines;
  for my $Card (@$Objects) {
    # We group properties in the same group together, track if we've
    #  already output a property
    my %DoneProps;

    my $Props = $Card->{properties};

    # Remove synthetic properties
    delete $Props->{online};

    # Order the properties
    my @PropKeys = sort {
      ($PropOutputOrder{$a} // 1000) <=> ($PropOutputOrder{$b} // 1000)
        || $a cmp $b
    } keys %$Props;

    # Generate output list
    my @OutputProps;
    for my $PropKey (@PropKeys) {
      my @PropVals = @{$Props->{$PropKey}};
      for my $PropVal (@PropVals) {
        next if $DoneProps{"$PropVal"}++;

        push @OutputProps, $PropVal;

        # If it has a group, output all values in that group together
        if (my $Group = $PropVal->{group}) {
          push @OutputProps, grep { !$DoneProps{"$_"}++ } @{$Card->{groups}->{$Group}};
        }
      }
    }

    my $Type = $Card->{type};
    push @Lines, ("BEGIN:" . $Type);

    for (@OutputProps) {
      next if $_->{deleted};

      my $Binary = $_->{binary};
      if ($Binary) {
        my $Encoding = ($_->{params}->{encoding} //= []);
        push @$Encoding, "b" if !@$Encoding;
      }

      my $LName = $_->{name};
      my $Group = $_->{group};

      # rfc6350 3.3 - it is RECOMMENDED that property and parameter names be upper-case on output.
      my $Line = ($Group ? (uc $Group . ".") : "") . uc $LName;

      while (my ($Param, $ParamVals) = each %{$_->{params}}) {
        if (!defined $ParamVals) {
          $Line .= ";" . uc($Param);
        }
        for my $ParamVal (ref($ParamVals) ? @$ParamVals : $ParamVals) {
          $ParamVal =~ s/\n/\\N/g if $Param eq 'label';
          $ParamVal =~ s/([\n^"])/'^' . $RFC6868RevMap{$1}/ge;
          $ParamVal = '"' . $ParamVal . '"' if $ParamVal =~ /\W/;
          $Line .= ";" . uc($Param) . "=" . $ParamVal;
        }
      }
      $Line .= ":";

      my $Value = $_->{values} || $_->{value};

      if ($_->{binary}) {
        $Value = encode_base64($Value, '');

      } else {
        for (ref $Value ? @$Value : $Value) {
          $_ //= '';
          # rfc6350 3.4 (v4, assume clarifies many v3 semantics)
          # - a SEMICOLON in a field of such a "compound" property MUST be
          #   escaped with a BACKSLASH character
          # - a COMMA character in one of a field's values MUST be escaped
          #   with a BACKSLASH character
          # - BACKSLASH characters in values MUST be escaped with a BACKSLASH
          #   character.
          s/([\,\;\\])/\\$1/g;
          # - NEWLINE (U+000A) characters in values MUST be encoded
          #   by two characters: a BACKSLASH followed by either an 'n' (U+006E)
          #   or an 'N' (U+004E).
          s/\n/\\n/g;
        }

        if (ref $Value) {
          $Value = join ";", @$Value;
        } else {
          # Stripped v4 proto prefix, add it back
          if (my $ProtoStrip = $_->{proto_strip}) {
            $Value = $ProtoStrip . $Value;
          }
        }

        # If it's a perl unicode string, make it utf-8 bytes
        #if (utf8::is_utf8($Value)) {
          #$Value = encode_utf8($Value);
        #}
      }

      $Line .= $Value;

      push @Lines, foldline($Line);
    }

    push @Lines, "END:" . $Type;
  }

  return @Lines;
}

sub foldline {
  local $_ = shift;

  # Try folding on at whitespace boundaries first
  # Otherwise fold to 75 chars, but don't split utf-8 unicode char
  my @Out;
  while (/\G(.{60,75})(?<=[^\n\t ])(?=[\n\t ])/gc || /\G(.{1,75})(?![\x80-\xbf])/gc) {
    push @Out, (@Out ? " " . $1 : $1);
  }
  push @Out, " " . substr($_, pos($_)) if pos $_ != length $_;

  return @Out;
}

# }}}

1;

1;
__END__
# Below is stub documentation for your module. You'd better edit it!

=head1 NAME

Text::VCardFast - Perl extension for blah blah blah

=head1 SYNOPSIS

  use Text::VCardFast;
  blah blah blah

=head1 DESCRIPTION

Stub documentation for Text::VCardFast, created by h2xs. It looks like the
author of the extension was negligent enough to leave the stub
unedited.

Blah blah blah.

=head2 EXPORT

None by default.



=head1 SEE ALSO

Mention other useful documentation such as the documentation of
related modules or operating system documentation (such as man pages
in UNIX), or any relevant external documentation such as RFCs or
standards.

If you have a mailing list set up for your module, mention it here.

If you have a web site set up for your module, mention it here.

=head1 AUTHOR

Bron Gondwana, E<lt>brong@E<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2014 by Bron Gondwana

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.14.2 or,
at your option, any later version of Perl 5 you may have available.


=cut
