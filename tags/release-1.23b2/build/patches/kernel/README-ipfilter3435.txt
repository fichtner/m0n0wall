
		Changes to IPFilter 3.4.35
		--------------------------

1) The BSD version conditionals in the definitions of IFNAME and struct ipflog
have been updated to handle later FreeBSD 5.x versions correctly.  FreeBSD was
the last BSD variant to incorporate the change from the if_name/if_unit to
if_xname in naming interfaces, and the change wasn't taken into account in all
places.  The affected files are ip_compat.h and ip_fil.h.  Note that there may
be additional fixes for this needed in ip_fil.c, but they only appear to relate
to the userland build.

2) The include of FreeBSD's opt_ipfilter.h in fil.c was too late to override
default parameters, so it was moved earlier.

3) M0n0wall's "forced MSS clamping" hack has been incorporated under the
conditional IPFILTER_MSSCLAMP_FORCE, which defaults off.  The affected files
are ip_nat.h, ip_nat.c, mlfk_ipl.c, and mlf_ipl.c.

4) The window scaling bug previously fixed in 3.4.33 has been fixed again.  The
affected file is ip_state.c.

5) The code for adjusting checksums in NATted ICMP errors has been fixed again,
since it was still failing in some cases.  The affected file is ip_nat.c.

6) The NAT checksum adjustment routines have been fixed to perform a normal sum,
rather than doing the computation "upside down".  This prefers the -0 result,
and therefore doesn't risk adjusting a UDP checksum to "disabled".  Either form
of zero is acceptable for non-UDP cases.

7) The filter code no longer treats the ICMP sequence number as part of the key
for the state entry.  This means that a sequence of pings now uses a single
state entry (unless the pings are spaced farther apart than the state lifetime),
and the stats in the entry reflect the ongoing stream.  This behavior avoids
keeping multiple state entries for a single ping stream, including potentially
filling the entire state table during flood pings.

8) Since ICMP state entries are now usefully recycled, the default "ack" timeout
has been increased to the same 60 seconds as the default request timeout.

9) The code for matching ICMP (v4) query replies against requests now handles
all four supported reply types, rather than just echo reply.


		Notes on ICMP Checksum Issues
		-----------------------------

The NAT ICMP error checksum adjustments have been the subject of many rounds of
tweaking, and still weren't right.  Even some workimng cases were being handled
in an unnecessarily roundabout and confusing way (e.g. adding double corrections
when the real problem was that the correction had originally been applied in the
wrong direction.  The code has been reworked more than minimally, but less than
it really should be.  The general flow (for the embedded packet) is:

1) The IP address difference is applied (oppositely) to the IP header checksum. 
It is not directly applied to the ICMP checksum, since the header checksum
change cancels the address change.  To put it another way, all valid IP headers
have an overall checksum of 0, so any change that transforms one valid IP header
into another is guaranteed to be checksum-neutral.

2) For TCP and UDP, the IP address change is applied to the TCP/UDP checksum (if
present) due to its effect on the pseudo-header, and any such adjustment is
applied (oppositely) to the ICMP checksum in compensation. This does not require
"observing" the TCP/UDP checksum change, since the difference is precisely the
correction just applied.  For UDP, "present" means not being +0, while for TCP,
"present" means being within the included portion of the offending packet.

3) For TCP and UDP, any port number change is applied (oppositely) to the ICMP
checksum, to compensate the change in the port number field.

4) For TCP and UDP, any port number change is applied (oppositely) to the
TCP/UDP checksum (if present), and any such change is applied (non-oppositely)
to the ICMP checksum.  If present, this adjustment cancels the effect of #3.

5) The accumulated ICMP checksum adjustment is applied, without any extra
complement or bizarre direction-dependent increment.


		Notes on General Checksum Issues
		--------------------------------

Since the ones-complement representation has two possible zero values (0 and
~0), implementations vary as to which zero result is produced in which cases. In
fact, hardware implementations are actually nondeterministic in this regard
without special logic to force a preference.  The only IP-related checksum whose
zero value is precisely specified is the UDP checksum, where the +0 value is
reserved for "none", requiring the ~0 form to be used for "real" zero.

The most common software implementation of ones-complement add produces the ~0
result in almost all cases, so the "complement of the sum" language in the
specification of various IP-related checksums *could* be construed as preferring
the +0 form.  But since it doesn't explicitly specify the zero preference of the
underlying sum, that can't necessarily be assumed.  The real intent of the
checksum definition is to provide a value which causes the overall checksum of
the entire set of bytes (including the checksum) to be zero, hence making the
checksum the complement of the sum of everything else.  This condition is met by
either form of zero, something which is mentioned in the discussion of the UDP
checksum in RFC1122.

It's also worth noting that if an implementation used the same checksum check
code for non-UDP checksums as for UDP checksums, it might erroneously regard
+0 non-UDP checksums as absent.  While this behavior is clearly incorrect, it
can be avoided by preferring ~0 checksums for non-UDP cases as well.

Thus, an argument can be made for using the ~0 representation for zero checksums
in all cases, which is also the natural result of using a UDP-compatible
calculation in other places.  The only way to prefer +0 for non-UDP checksums
while generating the required ~0 in the UDP case would be to use different
calculations for UDP and non-UDP cases, which is almost certainly not necessary
and probably not desirable.

With regard to the meaning of "prefer", let's use "@" to represent ones-
complement addition.  For any "natural" @ operation, the three cases that
produce mathematically zero results are as follows:

	+0 @ +0 -> +0 always
	~0 @ ~0 -> ~0 always
	 x @ ~x -> +0 or ~0, depending on implementation

The most common form (end-around carry initially presumed false) prefers the ~0
result in the last case, meaning that the only time the result can be +0 is when
all summands are +0.  Thus, as long as at least one bit in the checksummed area
can be guaranteed nonzero, the normal calculation can be used to produce the ~0
form of zero without any special check.


Note that the proper way to compute a ones-complement difference is to compute a
ones-complement sum using the *ones* complement of the subtrahend.  I.e the
ones-complement equivalent of (x - y) is (x @ ~y).  Twos-complement subtraction
can't be used unless an "end-around borrow" is also included, and the result
then has a +0 preference.


As noted in RFC1071, all checksum calculations can be performed in network byte
order on any processor, althought the unnecessary byte swapping hasn't been
removed from IPFilter.

					Fred Wright
					fw@well.com
					5-Apr-2005
