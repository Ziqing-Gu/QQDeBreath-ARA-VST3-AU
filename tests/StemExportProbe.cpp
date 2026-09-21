#include "shared/StemExport.h"
#include "LegacyStemExport123.h"
#include <iostream>
#include <random>
#include <chrono>
#include <stdexcept>
using namespace QQDeBreathStemExport;
namespace {
void require(bool condition, const char* what) { if (!condition) throw std::runtime_error(what); }
juce::AudioBuffer<float> source(int samples, int channels = 2)
{
    juce::AudioBuffer<float> audio(channels, samples);
    std::mt19937 rng(123);
    for (int c=0;c<channels;++c) for(int n=0;n<samples;++n)
        audio.setSample(c,n,static_cast<float>(std::sin(n*0.019+c)*0.22 + static_cast<double>(rng()%1000)/10000.0-0.05));
    return audio;
}
juce::Array<QQDeBreathBridgeRegion> regionsFor(int samples, int count)
{
    juce::Array<QQDeBreathBridgeRegion> regions;
    for(int i=0;i<count;++i) {
        QQDeBreathBridgeRegion r; r.type = i%3==0 ? "Noize" : "Breath";
        r.startSample=i*(samples/count); r.endSample=juce::jmin<juce::int64>(samples,r.startSample+samples/count/3);
        r.gainDb=(i%5-2)*3.0; regions.add(r);
    } return regions;
}
double difference(const Stems& a,const Stems& b)
{
    double error=0;
    const std::array<const juce::AudioBuffer<float>*,3> x{&a.vocal,&a.breath,&a.noize},y{&b.vocal,&b.breath,&b.noize};
    for(size_t i=0;i<3;++i)for(int c=0;c<x[i]->getNumChannels();++c)for(int n=0;n<x[i]->getNumSamples();++n)
        error=juce::jmax(error,std::abs(static_cast<double>(x[i]->getSample(c,n))-y[i]->getSample(c,n)));
    return error;
}
Request requestFor(const juce::File& directory, int samples=96000)
{
    Request r; r.directory=directory;r.sampleRate=48000;r.source=source(samples);r.regions=regionsFor(samples,40);return r;
}
void wait(Job& job) { require(job.waitForThreadToExit(30000),"worker timeout");require(job.finished(),"completion not published"); }
void checkFiles(const juce::File& directory,const Stems& expected,int samples)
{
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    const std::array<juce::String,3> names{"Vocal Only.wav","Breath.wav","Noize.wav"};
    const std::array<const juce::AudioBuffer<float>*,3> buffers{&expected.vocal,&expected.breath,&expected.noize};
    for(size_t i=0;i<3;++i){
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(directory.getChildFile(names[i])));
        require(reader!=nullptr,"missing wav");require(reader->lengthInSamples==samples && reader->sampleRate==48000 && reader->numChannels==2,"wav metadata");
        juce::AudioBuffer<float> data(2,samples);require(reader->read(&data,0,samples,0,true,true),"read exported wav");
        for(int c=0;c<2;++c)for(int n=0;n<samples;++n)
            require(std::abs(data.getSample(c,n)-buffers[i]->getSample(c,n))<2e-7f,"wav samples differ");
    }
}
}
int main(int argc,char** argv)
{
    if(argc!=2){std::cerr<<"Provide a D-drive test output directory\n";return 2;}
    try {
        const juce::File directory(juce::String::fromUTF8(argv[1]));require(directory.createDirectory().wasOk(),"test directory");
        double worst=0;
        for(double rate:{44100.0,48000.0,96000.0})for(int mode=0;mode<12;++mode){
            auto audio=source(12000,mode%2+1);auto regions=regionsFor(12000,12);Settings settings;
            settings.enableFade=mode!=0;settings.normalizeBreath=mode%2==0;settings.fadeInMs=mode==2?0:0.7;settings.fadeOutMs=mode==3?0:1.3;
            settings.breathGainDb=-4;settings.breathTargetDb=-12;
            if(mode>=4){regions.getReference(1).startSample=3900;regions.getReference(1).endSample=6000;
                regions.getReference(2).startSample=3900;regions.getReference(2).endSample=6000;
                regions.getReference(3).startSample=6000;regions.getReference(3).endSample=6020;
                regions.getReference(4).startSample=5990;regions.getReference(4).endSample=6010;}
            if(mode>=6){settings.globalEq.enabled=true;settings.globalEq.highPassEnabled=true;
                settings.globalEq.bands[0].enabled=true;settings.globalEq.bands[0].gainDb=-4;
                regions.getReference(1).eqState=settings.globalEq;regions.getReference(4).eqState=settings.globalEq;}
            if(mode==7)audio.clear();
            if(mode==8)regions.clear();
            if(mode==9){regions.getReference(5).startSample=0;regions.getReference(5).endSample=0;
                regions.getReference(5).startTime=0.05;regions.getReference(5).endTime=0.10;}
            if(mode==10){regions.getReference(7).endSample=regions.getReference(7).startSample;}
            if(mode==11){regions.getReference(7).endSample=regions.getReference(7).startSample-30;}
            Stems oldStems,newStems;LegacyStemExport123::render(audio,rate,regions,settings,oldStems);
            require(render(audio,rate,regions,settings,newStems,[]{return false;},[](float){}),"render failed");
            const auto error=difference(oldStems,newStems);worst=juce::jmax(worst,error);require(error<=1e-7,"legacy sample mismatch");
        }
        std::cout<<"36 legacy equivalence cases passed; max sample error="<<worst<<std::endl;
        {
            auto audio=source(48000);auto regions=regionsFor(48000,100);Settings settings;settings.fadeInMs=settings.fadeOutMs=1;
            Stems oldStems,newStems;auto a=std::chrono::steady_clock::now();LegacyStemExport123::render(audio,48000,regions,settings,oldStems);
            auto b=std::chrono::steady_clock::now();require(render(audio,48000,regions,settings,newStems,[]{return false;},[](float){}),"benchmark render");auto c=std::chrono::steady_clock::now();
            require(difference(oldStems,newStems)<=1e-7,"benchmark equivalence");
            std::cout<<"1 sec / 100 regions legacy="<<std::chrono::duration<double>(b-a).count()<<"s candidate="<<std::chrono::duration<double>(c-b).count()<<"s"<<std::endl;
        }
        {
            auto audio=source(48000*120);auto regions=regionsFor(audio.getNumSamples(),1000);Settings settings;Stems stems;
            auto start=std::chrono::steady_clock::now();require(render(audio,48000,regions,settings,stems,[]{return false;},[](float){}),"long render");
            std::cout<<"120 sec / 1000 regions render="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"s"<<std::endl;
            int polls=0;require(!render(audio,48000,regions,settings,stems,[&]{return ++polls>100;},[](float){}),"render cancellation");
        }
        auto request=requestFor(directory.getChildFile("recorded"));Stems expected;
        LegacyStemExport123::render(request.source,request.sampleRate,request.regions,request.settings,expected);
        {Job job(std::move(request));require(job.startThread(),"start worker");wait(job);require(job.succeeded,"export failed");checkFiles(directory.getChildFile("recorded"),expected,96000);}
        {
            auto r=requestFor(directory.getChildFile("loaded-wav"));r.source.setSize(0,0);
            juce::AudioFormatManager formats;formats.registerBasicFormats();
            // Use a real local wav as the ARA snapshot input, independent of any ARA host.
            r.reader.reset(formats.createReaderFor(directory.getChildFile("recorded/Vocal Only.wav")));
            require(r.reader!=nullptr,"reader setup");
            juce::AudioBuffer<float> loaded(2,96000);require(r.reader->read(&loaded,0,96000,0,true,true),"source read");
            Stems loadedExpected;LegacyStemExport123::render(loaded,48000,r.regions,r.settings,loadedExpected);
            Job job(std::move(r));require(job.startThread(),"start wav worker");wait(job);require(job.succeeded,"wav export failed");checkFiles(directory.getChildFile("loaded-wav"),loadedExpected,96000);
        }
        {
            const auto dir=directory.getChildFile("cancel");require(dir.createDirectory().wasOk(),"cancel directory");
            for(const auto* name:{"Vocal Only.wav","Breath.wav","Noize.wav"})require(dir.getChildFile(name).replaceWithText("keep existing"),"seed existing");
            Job job(requestFor(dir,48000*30));require(job.startThread(),"cancel start");
            const auto deadline=juce::Time::getMillisecondCounter()+10000;
            while(job.getProgress()<0.75f && !job.finished() && juce::Time::getMillisecondCounter()<deadline)juce::Thread::sleep(1);
            job.signalThreadShouldExit();wait(job);require(!job.succeeded,"cancelled task completed");
            for(const auto* name:{"Vocal Only.wav","Breath.wav","Noize.wav"})require(dir.getChildFile(name).loadFileAsString()=="keep existing","cancel overwrote target");
            require(dir.getNumberOfChildFiles(juce::File::findFiles)==3,"temporary file leak");
        }
        {
            const auto invalid=directory.getChildFile("blocked-directory");require(invalid.replaceWithText("file"),"invalid setup");
            Job job(requestFor(invalid));require(job.startThread(),"error start");wait(job);require(!job.succeeded && job.status.isNotEmpty(),"missing write error");
        }
        {
            auto job=std::make_unique<Job>(requestFor(directory.getChildFile("close"),48000*30));
            require(job->startThread(),"close start");job.reset();
        }
        std::cout<<"PASS: recorded/local-wav background export, WAV samples, cancellation preserves files, cleanup, write failure and worker destruction"<<std::endl;
        return 0;
    } catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<std::endl;return 1;}
}
