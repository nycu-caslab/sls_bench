SparseLengthsSum (SLS) Benchmark
===
# Outline
* Environment Setup
    * pytorch
    * clang
    * bcc
* Dataset
    * DLRM
    * Kaggle
* Architecture
    * main programs
    * biopattern
    * fixed
    * emb
* Experiment Results

# Environment Setup
## pytorch - build env. for dlrm
dlrm: https://github.com/facebookresearch/dlrm
1. 用Miniconda裝，不要用source code內的requirement.txt裝
```=
conda install pytorch==1.10.1 torchvision==0.11.2 torchaudio==0.10.1 cudatoolkit=11.3 -c pytorch -c conda-forge
```
conda torch裝好後，跑dlrm_s_caffe2.py
遇到啥問題再裝
必裝：
pip3 install future
[mlperf-logging](https://github.com/mlcommons/logging)
[onnx](https://github.com/onnx/onnx)

2. 用docker裝
```=
docker pull burniechen/dlrm:latest
```
記得裝docker-nvidia: https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/install-guide.html
不然你就只能用cpu跑

## clang - compile programs
Ubuntu: https://apt.llvm.org

## bcc - BPF compiler collection
安裝流程：https://github.com/iovisor/bcc/blob/v0.24.0/INSTALL.md
```=
git clone --branch v0.24.0 https://github.com/iovisor/bcc.git
sudo apt install -y bison build-essential cmake flex git libedit-dev libllvm7 llvm-7-dev libclang-7-dev python zlib1g-dev libelf-dev libfl-dev python3-distutils
```
用v0.24.0比較穩

# Dataset
存embedding tables的方式可以搜尋emb/dlrm_s_pytorch.py的"# burnie"
## DLRM
用標準的rmc1,2,3來產生embedding tables
1. dlrm_rm1.json
![](https://i.imgur.com/Qja4fFl.png)

```=bash
python3 dlrm_s_pytorch.py --arch-mlp-bot=128-64-32 --arch-mlp-top=256-64-1 --arch-embedding-size=4000000-4000000-4000000-4000000-4000000-4000000-4000000-4000000 --arch-sparse-feature-size=32 --num-indices-per-lookup-fixed=true --num-indices-per-lookup=80 --arch-interaction-op=cat
```

### rm1
![](https://i.imgur.com/HHBMA20.png)


2. dlrm_rm2.json
![](https://i.imgur.com/iXRLEhc.png)

```=bash
python3 dlrm_s_pytorch.py --arch-mlp-bot=256-128-64 --arch-mlp-top=128-64-1 --arch-embedding-size=500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000-500000 --arch-sparse-feature-size=64 --num-indices-per-lookup-fixed=true --num-indices-per-lookup=120 --arch-interaction-op=cat
```

### rm2
![](https://i.imgur.com/XajsElT.png)


3. dlrm_rm3.json
![](https://i.imgur.com/VEOHhDB.png)

```=bash
python3 dlrm_s_pytorch.py --arch-mlp-bot=2560-1024-256-32 --arch-mlp-top=512-256-1 --arch-embedding-size=2000000-2000000-2000000-2000000-2000000-2000000-2000000-2000000-2000000-2000000 --arch-sparse-feature-size=32 --num-indices-per-lookup-fixed=true --num-indices-per-lookup=20 --arch-interaction-op=cat
```
### rm3

![](https://i.imgur.com/SaKTvAW.png)

### Fearture Cal.
* emb_l → 8 (emb. table) * 32 (after EmbeddingBag) = 256
* bot_l → last layer, out_features = 32
* top_l, first layer, in_features → 256 + 32 = 288

## Kaggle
dataset: https://figshare.com/articles/dataset/Kaggle_Display_Advertising_Challenge_dataset/5732310/1
先解壓縮到dac_big/然後再跑以下程式

```=
python3 dlrm_s_pytorch.py \
        --arch-sparse-feature-size=16 \
        --arch-mlp-bot="13-512-256-64-16" \
        --arch-mlp-top="512-256-1" \
        --data-generation=dataset \
        --data-set=kaggle \
        --raw-data-file=./dac_big/train.txt \
        --loss-function=bce \
        --round-targets=True \
        --learning-rate=0.1 \
        --mini-batch-size=128\
        --print-freq=1024 \
        --print-time \
        --test-mini-batch-size=16384 \
        --test-num-workers=16 \
        --use-gpu 
```
可以參考dlrm/bench/

# Architecture
## main programs
跑test.sh來build程式，內部有範例程式
1. bench: 測量所有I/O在指定資料夾內所有的embedding tables跑sls的時間。跑之前要設定總共跑幾次，batch size，table size
2. each: 測量指定I/O在指定資料夾內所有的embedding tables跑sls的時間。跑之前要設定總共跑幾次，batch size，table size
3. ratio: 測量預載入不同比例的table，在指定資料夾內所有的embedding tables跑sls的時間。跑之前要設定總共跑幾次，batch size，table size
* 最好用clang++搭配c+ +20
* 可以在src/sls.hpp設定gen_id的方式，true→uniform / false→2-peak binomial
* sls的實作都在src/sls.cpp
* 測量時間都在src/bench.hpp，總共分三階段，初始，測量，結束，時間只會計算"測量"階段做的事情。初始階段我會清掉page cache內的data以及紀錄log，測量階段就做sls，結束階段紀錄log
* 最後都會匯出csv檔

### DEMO
![](https://i.imgur.com/HsLenJN.png)

## biopattern
bcc reference guide: https://github.com/iovisor/bcc/blob/master/docs/reference_guide.md#4-bpf_get_current_pid_tgid

linux/blk_types.h:
https://elixir.bootlin.com/linux/latest/source/include/linux/blk_types.h

基本上看看api怎麼操作還有一些linux macro的定義，應該就就能寫出來了

### DEMO
![](https://i.imgur.com/FbZDwTv.png)

## fixed
可以先寫死一些id，來測量io_unbuf跟opt的答案

## emb
裡面有修改好的dlrm_s_pytorch.py可以來產生embedding tables
embedding tables內部的值都是用double來存

# Experiment Results
https://docs.google.com/spreadsheets/d/1d8l1pqjJc_rHbeY2MANvhuBB9WRoIALmBN-I_2kNblQ/edit?usp=sharing
