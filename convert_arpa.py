"""convert arpa to more friendly format
Usage: convert_arpa.py <arpa>
"""
__author__ = 'chenkovsky'
#import config
from rex import rex
import sys
from arpa import arpa

if __name__ == '__main__':
    from docopt import docopt
    import gzip
    def gram_func(lm_info, section, words, prob, bow):
        print("%d\t%s\t%d\t%d" % (section, "\t".join(words), int(prob*1000000), int(bow*1000000)))
    def header_end_func(lm_info):
        lm_info = sorted(list(lm_info.items()), key=lambda x: x[0])
        print(len(lm_info))
        for k,v in lm_info:
            print(v)
    arguments = docopt(__doc__, version="convert_arpa 1.0")
    with gzip.open(arguments["<arpa>"],"rt") as fi:
        arpa(fi, gram= gram_func, header_end=header_end_func)
