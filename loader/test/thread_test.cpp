/*
 Copyright 2016 Nervana Systems Inc.
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

#include "loader.cpp"

// Code for unit testing.

unsigned int sum(char* data, unsigned int len) {
    unsigned int result = 0;
    for (unsigned int i = 0; i < len; i++) {
        result += data[i];
    }
    return result;
}

int single(Loader* loader, int epochCount, int minibatchCount,
           int batchSize, int datumSize, int targetSize,
           ImageParams* mediaParams, ImageIngestParams* ingestParams) {
    unsigned int sm = 0;
    Reader* reader = loader->getReader();
    Media* media = Media::create(mediaParams, ingestParams, 0);
    char* dataBuf = new char[datumSize];
    memset(dataBuf, 0, datumSize);
    CharBuffer dataBuffer(0);
    CharBuffer targetBuffer(0);
    BufferTuple bufTuple = make_tuple(&dataBuffer, &targetBuffer, (IntBuffer*)0);
    for (int epoch = 0; epoch < epochCount; epoch++) {
        reader->reset();
        CharBuffer* first = get<0>(bufTuple);
        CharBuffer* second = get<1>(bufTuple);;

        for (int i = 0; i < minibatchCount; i++) {
            first->reset();
            second->reset();
            reader->read(bufTuple);
            for (int j = 0; j < batchSize; j++) {
                int itemSize = 0;
                char* item = first->getItem(j, itemSize);
                assert(item != 0);
                media->transform(item, itemSize, dataBuf, datumSize, 0);
                sm += sum(dataBuf, datumSize);
                int targetChunkSize = 0;
                char* targets = second->getItem(j, targetChunkSize);
                sm += sum(targets, targetSize);
            }
        }
    }

    delete[] dataBuf;
    delete media;
    return sm;
}

int multi(Loader* loader, int epochCount, int minibatchCount,
          int batchSize, int datumSize, int targetSize) {
    int result = loader->start();
    assert(result == 0);
    unsigned int sm = 0;
    int dataBufSize = batchSize * datumSize;
    int targetsBufSize = batchSize * targetSize;
    CharBuffer data(dataBufSize);
    CharBuffer targets(targetsBufSize);
    data.init();
    targets.init();
    Device* device = loader->getDevice();
    for (int epoch = 0; epoch < epochCount; epoch++) {
        loader->reset();
        for (int i = 0; i < minibatchCount; i++) {
            loader->next();
            int bufIdx = i % 2;
            device->copyDataBack(bufIdx, &data);
            device->copyLabelsBack(bufIdx, &targets);
            sm += sum(data._data, dataBufSize);
            sm += sum(targets._data, targetsBufSize);
        }
    }
    loader->stop();
    return sm;
}

int test(char* repoDir, char* indexFile,
         int batchSize, int nchan, int height, int width) {
    int datumSize = nchan * height * width;
    int targetSize = 1;
    int datumTypeSize = 1;
    int targetTypeSize = 4;
    int targetConversion = 1;
    int epochCount = 2;
    int minibatchCount = 65;
    int itemCount = 0;
    int datumLen = datumSize * datumTypeSize;
    int targetLen = targetSize * targetTypeSize;

    ImageParams mediaParams(nchan, height, width, true, false, 0, 0, 100, 100,
                            0, 0, 0, false, 0, 0, 0, 0);
    char* dataBuffer[2];
    char* targetBuffer[2];
    int* meta[2];
    for (int i = 0; i < 2; i++) {
        dataBuffer[i] = new char[batchSize * datumLen];
        targetBuffer[i] = new char[batchSize * targetLen];
        meta[i] = 0;
    }

    string archiveDir(repoDir);
    archiveDir += "-ingested";
    CpuParams deviceParams(0, 0, dataBuffer, targetBuffer, meta);
    ImageIngestParams ingestParams(false, true, 0, 0);
    Loader loader(&itemCount, batchSize, repoDir, archiveDir.c_str(),
                  indexFile, "archive-",
                  false, false, 0, datumSize, datumTypeSize,
                  targetSize, targetTypeSize, targetConversion, 100,
                  &mediaParams, &deviceParams, &ingestParams, 0);
    unsigned int singleSum = single(&loader, epochCount,
                                    minibatchCount, batchSize,
                                    datumLen, targetLen,
                                    &mediaParams, &ingestParams);
    unsigned int multiSum = multi(&loader, epochCount,
                                  minibatchCount, batchSize,
                                  datumLen, targetLen);
    for (int i = 0; i < 2; i++) {
        delete[] dataBuffer[i];
        delete[] targetBuffer[i];
    }
    printf("sum %u true sum %u\n", multiSum, singleSum);
    assert(multiSum == singleSum);
    printf("OK\n");
    return 0;
}

int main(int argc, char** argv) {
    int nchan = 3;
    int height = 32;
    int width = 32;
    int batchSize = 128;
    if (argc < 3) {
        printf("Usage: %s repo_dir index_file\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char* repoDir = argv[1];
    char* indexFile = argv[2];

    test(repoDir, indexFile, batchSize, nchan, height, width);
}
