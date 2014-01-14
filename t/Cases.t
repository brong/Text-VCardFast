# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl Text-VCardFast.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;
use FindBin qw($Bin);
use Test::More;
use JSON::XS;

BEGIN { use_ok('Text::VCardFast') };

my @tests;
if (opendir(DH, "$Bin/cases")) {
    while (my $item = readdir(DH)) {
	next unless $item =~ m/^(.*)\.vcf$/;
	push @tests, $1;
    }
    closedir(DH);
}

my $numtests = @tests;
plan tests => ($numtests * 8) + 2;

ok($numtests, "we have $numtests cards to test");

foreach my $test (@tests) {
    my $vdata = getfile("$Bin/cases/$test.vcf");
    ok($vdata, "data in $test.vcf");
    my $chash = eval { Text::VCardFast::vcard2hash_c($vdata, { multival => ['adr','org','n'] }) };
    ok($chash, "parsed VCARD in $test.vcf with C ($@)");
    my $phash = eval { Text::VCardFast::vcard2hash_pp($vdata, { multival => ['adr','org','n'] }) };
    ok($phash, "parsed VCARD in $test.vcf with pureperl ($@)");

    is_deeply($chash, $phash, "contents of $test.vcf match from C and pureperl");
#    warn encode_json($chash);

    my $jdata = getfile("$Bin/cases/$test.json");
    ok($jdata, "data in $test.json");
    my $jhash = eval { decode_json($jdata) };
    ok($jhash, "valid JSON in $test.json ($@)");

    is_deeply($chash, $jhash, "contents of $test.vcf match $test.json");

    my $data = Text::VCardFast::hash2vcard($chash);
    my $rehash = Text::VCardFast::vcard2hash($data, { multival => ['adr','org','n'] });

    is_deeply($chash, $rehash, "generated and reparsed data matches for $test");
}

sub getfile {
    my $file = shift;
    open(FH, "<$file") or return;
    local $/ = undef;
    my $res = <FH>;
    close(FH);
    return $res;
}

