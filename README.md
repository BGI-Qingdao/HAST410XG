# HAST
Partition 10XG reads based on trio-binning using prenatally unique markers.

## INSTALL

```
git clone https://github.com/BGI-Qingdao/HAST.git
cd HAST
make
```

## USAGE

```
Usage    :
./HAST410XG [OPTION]

Trio-phase filial 10XG reads based on paternal NGS reads and maternal NGS reads.

Options  :
        --paternal    paternal NGS reads file in fastq format.
                      ( note : gzip format IS NOT supported. )
        --maternal    maternal NGS reads file in fastq format.
                      ( note : gzip format IS NOT supported. )
        --filial      filial 10XG reads file in fastq format. only accept reads after longranger basic.
                      file in gzip format is accepted, but filename must end by ".gz".
        --thread      threads num.
                      [ optional, default 8 thread. ]
        --memory      x (GB) of memory to initial hash table by jellyfish.
                      ( note: real memory used maybe greater than this. )
                      [ optional, default 20GB. ]
        --jellyfish   jellyfish path.
                      [ optional, default jellyfish. ]
        --mer         mer-size
                      [ optional, default 21. ]
        --m-lower     maternal kmer count tablle will ignore mer with count < m-lower.
                      [ optional, default 9. ]
        --m-upper     maternal kmer count tablle will ignore mer with count > m-upper.
                      [ optional, default 33. ]
        --p-lower     paternal kmer count tablle will ignore mer with count < p-lower.
                      [ optional, default 9. ]
        --p-upper     paternal kmer count tablle will ignore mer with count > p-upper.
                      [ optional, default 33. ]
        --auto_bounds calcuate lower and upper bounds by kmercount table.
                      [ optional, default not trigger; no parameter. ]
                      ( note : if auto_bounds is open, it will overwrite --*-lower and --*-upper  ]
        --help        print this usage message.

Examples :
    ./HAST410XG --paternal father.fastq --maternal mater.fastq --filial son.fastq

    ./HAST410XG --paternal father.fastq --maternal mater.fastq --filial son.r1.fastq --filial son.r2.fastq

    ./HAST410XG --paternal father.fastq --maternal mater.fastq \
                     --filial son.r1.fastq --memory 20 --thread 20 \
                     --mer 21 --p-lower=9 --p-upper=33 --m-lower=9 --p-upper=33 \
                     --jellyfish /home/software/jellyfish/jellyfish-linux
```

## Input and Output details examples

*  scenario 01 ： full pipeine
        
      * Input 

       Parent's WGS sequences :  paternal.ngs.fastq  & maternal.ngs.fastq ;
       Children 10X Genomics sequences : barcoded.fastq.gz  ; NOTICE : 10XG raw reads are not supported , please run longranger basic command first.
      

      * Output
      
       maternal.barcoded.fastq
       paternal.barcoded.fastq
       homozygous.barcoded.fastq
      
## How to run haplotype denovo after HAST410GX

1. merge maternal.barcoded.fastq and homozygous.barcoded.fastq as maternal.final.fastq
      
        cat maternal.barcoded.fastq  homozygous.barcoded.fastq > maternal.final.fastq
     
2. merge paternal.barcoded.fastq and homozygous.barcoded.fastq as paternal.final.fastq 

        cat paternal.barcoded.fastq  homozygous.barcoded.fastq > paternal.final.fastq 
   
3. transfer format barck into 10X Genomics raw reads 
        
        mkdir maternal_supernova
        sh tools/barcode_fastq_gz_2_10xg_raw.sh maternal.final.fastq | gzip - >maternal_supernova/sample_S1_L001_RA_001.fastq.gz
        mkdir maternal_supernova
        sh tools/barcode_fastq_gz_2_10xg_raw.sh paternal.final.fastq | gzip - >paternal_supernova/sample_S1_L001_RA_001.fastq.gz 

4. run supernova independently.
       
       cd maternal_supernova && supernova run --fastqs ./ --ID maternal 
       cd paternal_supernova && supernova run --fastqs ./ --ID paternal 
       
Enjoy !
