#ifndef DNSSDINT_H
#define DNSSDINT_H

/* Use "dns-sd -Z _raop._tcp" to list found parameters */

#define RAOP_CN "0,1"       /* Audio codec: PCM, ALAC */
#define RAOP_DA "true"
#define RAOP_ET "0,3"           /* Encryption type: none, ?? */
#define RAOP_MD "0,1,2"         /* Metadata: text, artwork, progress */
#define RAOP_TP "UDP"
#define RAOP_VN "65537"
#define RAOP_VV "2"

#endif
