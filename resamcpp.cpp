/**
@file resamcpp.cpp
@author t-sakai

# License
This software is ported from the resampy.

## The resampy license

ISC License

Copyright (c) 2016, Brian McFee

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include "resamcpp.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#ifdef RESAMCPP_ISPC
#include "resamcpp.ispc.h"
#endif

namespace resamcpp
{
namespace
{
    template<class T>
    T minimum(T x0, T x1)
    {
        return x0 < x1 ? x0 : x1;
    }

    template<class T>
    T maximum(T x0, T x1)
    {
        return x0 < x1 ? x1 : x0;
    }

    template<class T>
    T clamp(T x, T minx, T maxx)
    {
        if(x<minx){
            return minx;
        }else if(maxx<x){
            return maxx;
        }else{
            return x;
        }
    }

#ifdef RESAMCPP_WAV
    bool read_fmt(WAVE& wave, FILE* file)
    {
        if(fread(&wave.format_, sizeof(FMT), 1, file) <= 0) {
            return false;
        }
        if(1 != wave.format_.format_) {
            return false;
        }
        if(wave.format_.channels_ <= 0 || 2 < wave.format_.channels_) {
            return false;
        }
        switch(wave.format_.bitsPerSample_) {
        case 8:
        case 16:
        case 24:
        case 32:
            break;
        default:
            return false;
        }
        return true;
    }

    bool read_data(WAVE& wave, u64 chunkSize, FILE* file)
    {
        u64 bytesPerSec = wave.format_.frequency_ * wave.format_.blockAlign_;
        if(bytesPerSec <= 0) {
            return false;
        }
        u64 numSamples = static_cast<u64>(static_cast<f64>(chunkSize) / bytesPerSec * wave.format_.frequency_);
        u64 size = numSamples * wave.format_.blockAlign_;
        wave.data_ = reinterpret_cast<u8*>(::malloc(size));
        if(RESAMCPP_NULL == wave.data_) {
            return false;
        }
        if(fread(wave.data_, size, 1, file) <= 0) {
            ::free(wave.data_);
            return false;
        }
        wave.numSamples_ = numSamples;
        return true;
    }
#endif
}

#ifdef RESAMCPP_WAV
bool WAVE::valid() const
{
    return RESAMCPP_NULL != data_;
}

WAVE load(const char* filepath)
{
    RESAMCPP_ASSERT(RESAMCPP_NULL != filepath);
    WAVE wave = {};
    FILE* file = fopen(filepath, "rb");
    if(RESAMCPP_NULL == file) {
        return wave;
    }
    bool loop = true;
    while(loop) {
        HEAD head;
        if(fread(&head, sizeof(HEAD), 1, file) <= 0) {
            break;
        }
        switch(head.id_) {
        case RIFF::ID: {
            RIFF riff;
            if(fread(&riff, sizeof(RIFF), 1, file) <= 0 || riff.format_ != RIFF::Format_Wave) {
                loop = false;
            }
        } break;
        case FMT::ID:
            if(!read_fmt(wave, file)) {
                loop = false;
            }
            break;
        case DATA::ID:
            read_data(wave, head.size_, file);
            break;
        }
    }
    fclose(file);
    return wave;
}

namespace
{
    struct Close
    {
        ~Close()
        {
            fclose(file_);
        }
        FILE* file_;
    };
} // namespace

bool save(const char* filepath, const WAVE& wave)
{
    RESAMCPP_ASSERT(RESAMCPP_NULL != filepath);
    FILE* file = fopen(filepath, "wb");
    if(RESAMCPP_NULL == file) {
        return false;
    }
    Close close = {file};
    HEAD head;
    u64 dataSize = wave.numSamples_ * wave.format_.blockAlign_;
    u64 totalSize = sizeof(RIFF) + sizeof(HEAD) + sizeof(FMT) + sizeof(HEAD) + dataSize;
    { // RIFF
        head.id_ = RIFF::ID;
        head.size_ = static_cast<u32>(totalSize);
        if(fwrite(&head, sizeof(HEAD), 1, file) <= 0) {
            return false;
        }
        RIFF riff = {RIFF::Format_Wave};
        if(fwrite(&riff, sizeof(RIFF), 1, file) <= 0) {
            return false;
        }
    }
    { // FMT
        head.id_ = FMT::ID;
        head.size_ = sizeof(FMT);
        if(fwrite(&head, sizeof(HEAD), 1, file) <= 0) {
            return false;
        }
        if(fwrite(&wave.format_, sizeof(FMT), 1, file) <= 0) {
            return false;
        }
    }

    {
        head.id_ = DATA::ID;
        head.size_ = static_cast<u32>(dataSize);
        if(fwrite(&head, sizeof(HEAD), 1, file) <= 0) {
            return false;
        }
        if(fwrite(wave.data_, dataSize, 1, file) <= 0) {
            return false;
        }
    }
    return true;
}

void destroy(WAVE& wave)
{
    ::free(wave.data_);
    wave = {};
}
#endif

namespace
{
    // clang-format off
const f32 KaiserFast[129] = {
static_cast<f32>(8.499999999999999778e-01), static_cast<f32>(8.340987492996022601e-01), static_cast<f32>(7.874817237653068958e-01), static_cast<f32>(7.133193033383873338e-01), static_cast<f32>(6.166043268285958945e-01), 
static_cast<f32>(5.037433166021544340e-01), static_cast<f32>(3.820372263464146823e-01), static_cast<f32>(2.590985046736161701e-01), static_cast<f32>(1.422564402710740916e-01), static_cast<f32>(3.800295451106208006e-02), 
static_cast<f32>(-4.847372103729211124e-02), static_cast<f32>(-1.136291666718088900e-01), static_cast<f32>(-1.557509303598278994e-01), static_cast<f32>(-1.749836375038000824e-01), static_cast<f32>(-1.731953173923218847e-01), 
static_cast<f32>(-1.537036940871576440e-01), static_cast<f32>(-1.208934245372049604e-01), static_cast<f32>(-7.976412029183363450e-02), static_cast<f32>(-3.545370033507769247e-02), static_cast<f32>(7.218130935140766591e-03), 
static_cast<f32>(4.414556740837451143e-02), static_cast<f32>(7.223867270033920707e-02), static_cast<f32>(8.960943455800829405e-02), static_cast<f32>(9.563201636348074575e-02), static_cast<f32>(9.087998722109594252e-02), 
static_cast<f32>(7.695504462429837711e-02), static_cast<f32>(5.623140839303890359e-02), static_cast<f32>(3.154679043100863667e-02), static_cast<f32>(5.874102559630261372e-03), static_cast<f32>(-1.799229292857106727e-02), 
static_cast<f32>(-3.770572504416478538e-02), static_cast<f32>(-5.156823341405535749e-02), static_cast<f32>(-5.864847727104856995e-02), static_cast<f32>(-5.881105487722142894e-02), static_cast<f32>(-5.266120135939462987e-02), 
static_cast<f32>(-4.141724069001181258e-02), static_cast<f32>(-2.672972331521002165e-02), static_cast<f32>(-1.047031537124061551e-02), static_cast<f32>(5.485060242212602341e-03), static_cast<f32>(1.945557122818850418e-02), 
static_cast<f32>(3.011765848698802484e-02), static_cast<f32>(3.661583043700127743e-02), static_cast<f32>(3.861439470272986368e-02), static_cast<f32>(3.628934265770055595e-02), static_cast<f32>(3.026606850360209072e-02), 
static_cast<f32>(2.151397957521417395e-02), static_cast<f32>(1.121283449937531505e-02), static_cast<f32>(6.075351376112679514e-04), static_cast<f32>(-9.131971506857397211e-03), static_cast<f32>(-1.703090284533520862e-02), 
static_cast<f32>(-2.239852931485096510e-02), static_cast<f32>(-2.487864735977438763e-02), static_cast<f32>(-2.445931918639121863e-02), static_cast<f32>(-2.144421750164015456e-02), static_cast<f32>(-1.639194208896666005e-02), 
static_cast<f32>(-1.003267813416712333e-02), static_cast<f32>(-3.173289702308694851e-03), static_cast<f32>(3.397705844709149846e-03), static_cast<f32>(8.994786128505863343e-03), static_cast<f32>(1.310123931903916031e-02), 
static_cast<f32>(1.541050714500104973e-02), static_cast<f32>(1.584062628205268677e-02), static_cast<f32>(1.452255100398716636e-02), static_cast<f32>(1.176599055529931916e-02), static_cast<f32>(8.008591740126264763e-03), 
static_cast<f32>(3.755646844225439193e-03), static_cast<f32>(-4.820719909357170404e-04), static_cast<f32>(-4.245226718190729491e-03), static_cast<f32>(-7.171691423526290431e-03), static_cast<f32>(-9.027107848387670372e-03), 
static_cast<f32>(-9.718231427821312149e-03), static_cast<f32>(-9.289215083550093899e-03), static_cast<f32>(-7.902898679639353691e-03), static_cast<f32>(-5.810677620555117044e-03), static_cast<f32>(-3.315479023215594026e-03), 
static_cast<f32>(-7.327188196118433880e-04), static_cast<f32>(1.646134982188931150e-03), static_cast<f32>(3.583524252907699381e-03), static_cast<f32>(4.916548178484244497e-03), static_cast<f32>(5.567090551683547169e-03), 
static_cast<f32>(5.541285202439585889e-03), static_cast<f32>(4.919509329994644319e-03), static_cast<f32>(3.839062505555647549e-03), static_cast<f32>(2.472306428511776046e-03), static_cast<f32>(1.003269972914902497e-03), 
static_cast<f32>(-3.944218334981119977e-04), static_cast<f32>(-1.575901615452450933e-03), static_cast<f32>(-2.437999478735780341e-03), static_cast<f32>(-2.925892868627905964e-03), static_cast<f32>(-3.033424714598279264e-03), 
static_cast<f32>(-2.797794983831996055e-03), static_cast<f32>(-2.289910505253298003e-03), static_cast<f32>(-1.602039425152251275e-03), static_cast<f32>(-8.345406859202779762e-04), static_cast<f32>(-8.333714043518143340e-05), 
static_cast<f32>(5.704888225629115191e-04), static_cast<f32>(1.068019367697101629e-03), static_cast<f32>(1.376273363136831709e-03), static_cast<f32>(1.488439244534648645e-03), static_cast<f32>(1.421004750094861241e-03), 
static_cast<f32>(1.208537707901302660e-03), static_cast<f32>(8.970581968601451040e-04), static_cast<f32>(5.369897668825879006e-04), static_cast<f32>(1.765972437756705923e-04), static_cast<f32>(-1.433631589483458076e-04), 
static_cast<f32>(-3.933036154236073238e-04), static_cast<f32>(-5.565710438726437529e-04), static_cast<f32>(-6.293798095637436309e-04), static_cast<f32>(-6.191903748303277319e-04), static_cast<f32>(-5.419678317269842381e-04), 
static_cast<f32>(-4.188320791982775518e-04), static_cast<f32>(-2.726130127603548390e-04), static_cast<f32>(-1.247596248017244667e-04), static_cast<f32>(7.061196959677690653e-06), static_cast<f32>(1.104806275007797438e-04), 
static_cast<f32>(1.790082307129259277e-04), static_cast<f32>(2.117564603983245635e-04), static_cast<f32>(2.124891944621316235e-04), static_cast<f32>(1.882350151771501211e-04), static_cast<f32>(1.477250981642127333e-04), 
static_cast<f32>(9.989653268683285612e-05), static_cast<f32>(5.265291928215289936e-05), static_cast<f32>(1.200639590128984472e-05), static_cast<f32>(-1.834894346725894645e-05), static_cast<f32>(-3.705156545068235885e-05), 
static_cast<f32>(-4.474961268482067833e-05), static_cast<f32>(-4.355089437696552183e-05), static_cast<f32>(-3.636981757389096982e-05), static_cast<f32>(-2.628865215979546915e-05), 
};
    // clang-format on

    // clang-format off
const f32 KaiserFastDiff[129] = {
static_cast<f32>(-1.59012507003977177e-02), static_cast<f32>(-4.66170255342953643e-02), static_cast<f32>(-7.41624204269195619e-02), static_cast<f32>(-9.67149765097914393e-02), static_cast<f32>(-1.12861010226441461e-01), 
static_cast<f32>(-1.21706090255739752e-01), static_cast<f32>(-1.22938721672798512e-01), static_cast<f32>(-1.16842064402542078e-01), static_cast<f32>(-1.04253485760012005e-01), static_cast<f32>(-8.64766755483541982e-02), 
static_cast<f32>(-6.51554456345167787e-02), static_cast<f32>(-4.21217636880190094e-02), static_cast<f32>(-1.92327071439721831e-02), static_cast<f32>(1.78832011147819769e-03), static_cast<f32>(1.94916233051642407e-02), 
static_cast<f32>(3.28102695499526836e-02), static_cast<f32>(4.11293042453713259e-02), static_cast<f32>(4.43104199567559420e-02), static_cast<f32>(4.26718312702184591e-02), static_cast<f32>(3.69274364732337448e-02), 
static_cast<f32>(2.80931052919646956e-02), static_cast<f32>(1.73707618576690870e-02), static_cast<f32>(6.02258180547245170e-03), static_cast<f32>(-4.75202914238480323e-03), static_cast<f32>(-1.39249425967975654e-02), 
static_cast<f32>(-2.07236362312594735e-02), static_cast<f32>(-2.46846179620302669e-02), static_cast<f32>(-2.56726878713783753e-02), static_cast<f32>(-2.38663954882013286e-02), static_cast<f32>(-1.97134321155937181e-02), 
static_cast<f32>(-1.38625083698905721e-02), static_cast<f32>(-7.08024385699321246e-03), static_cast<f32>(-1.62577606172858991e-04), static_cast<f32>(6.14985351782679907e-03), static_cast<f32>(1.12439606693828173e-02), 
static_cast<f32>(1.46875173748017909e-02), static_cast<f32>(1.62594079439694061e-02), static_cast<f32>(1.59553756134532196e-02), static_cast<f32>(1.39705109859759018e-02), static_cast<f32>(1.06620872587995207e-02), 
static_cast<f32>(6.49817195001325260e-03), static_cast<f32>(1.99856426572858625e-03), static_cast<f32>(-2.32505204502930773e-03), static_cast<f32>(-6.02327415409846523e-03), static_cast<f32>(-8.75208892838791677e-03), 
static_cast<f32>(-1.03011450758388589e-02), static_cast<f32>(-1.06052993617640479e-02), static_cast<f32>(-9.73950664446866440e-03), static_cast<f32>(-7.89893133847781141e-03), static_cast<f32>(-5.36762646951575648e-03), 
static_cast<f32>(-2.48011804492342253e-03), static_cast<f32>(4.19328173383168995e-04), static_cast<f32>(3.01510168475106408e-03), static_cast<f32>(5.05227541267349450e-03), static_cast<f32>(6.35926395479953672e-03), 
static_cast<f32>(6.85938843185842848e-03), static_cast<f32>(6.57099554701784470e-03), static_cast<f32>(5.59708028379671350e-03), static_cast<f32>(4.10645319053329697e-03), static_cast<f32>(2.30926782596188943e-03), 
static_cast<f32>(4.30119137051637040e-04), static_cast<f32>(-1.31807527806552041e-03), static_cast<f32>(-2.75656044868784721e-03), static_cast<f32>(-3.75739881517305439e-03), static_cast<f32>(-4.25294489590082557e-03), 
static_cast<f32>(-4.23771883516115607e-03), static_cast<f32>(-3.76315472725501261e-03), static_cast<f32>(-2.92646470533556094e-03), static_cast<f32>(-1.85541642486137994e-03), static_cast<f32>(-6.91123579433641777e-04), 
static_cast<f32>(4.29016344271218250e-04), static_cast<f32>(1.38631640391074021e-03), static_cast<f32>(2.09222105908423665e-03), static_cast<f32>(2.49519859733952302e-03), static_cast<f32>(2.58276020360375053e-03), 
static_cast<f32>(2.37885380180077443e-03), static_cast<f32>(1.93738927071876823e-03), static_cast<f32>(1.33302392557654512e-03), static_cast<f32>(6.50542373199302672e-04), static_cast<f32>(-2.58053492439612800e-05), 
static_cast<f32>(-6.21775872444941570e-04), static_cast<f32>(-1.08044682443899677e-03), static_cast<f32>(-1.36675607704387150e-03), static_cast<f32>(-1.46903645559687355e-03), static_cast<f32>(-1.39769180641301444e-03), 
static_cast<f32>(-1.18147978195433899e-03), static_cast<f32>(-8.62097863283329408e-04), static_cast<f32>(-4.87893389892125624e-04), static_cast<f32>(-1.07531845970373300e-04), static_cast<f32>(2.35629730766283210e-04), 
static_cast<f32>(5.07884478578698052e-04), static_cast<f32>(6.87871080101046728e-04), static_cast<f32>(7.67498739231973299e-04), static_cast<f32>(7.51203545485096529e-04), static_cast<f32>(6.53825962998092966e-04), 
static_cast<f32>(4.97530545134190110e-04), static_cast<f32>(3.08253995439730080e-04), static_cast<f32>(1.12165881397816936e-04), static_cast<f32>(-6.74344944397874037e-05), static_cast<f32>(-2.12467042193558581e-04), 
static_cast<f32>(-3.11479511041157556e-04), static_cast<f32>(-3.60068429977557203e-04), static_cast<f32>(-3.60392523106917335e-04), static_cast<f32>(-3.19960402724016400e-04), static_cast<f32>(-2.49940456475261489e-04), 
static_cast<f32>(-1.63267428449036429e-04), static_cast<f32>(-7.28087656910998780e-05), static_cast<f32>(1.01894347334158990e-05), static_cast<f32>(7.72225431033434937e-05), static_cast<f32>(1.23135752528706686e-04), 
static_cast<f32>(1.46219066437922713e-04), static_cast<f32>(1.47853387958630372e-04), static_cast<f32>(1.31820821761402166e-04), static_cast<f32>(1.03419430541102058e-04), static_cast<f32>(6.85276032121461838e-05), 
static_cast<f32>(3.27482296853986359e-05), static_cast<f32>(7.32734063807059972e-07), static_cast<f32>(-2.42541792849815024e-05), static_cast<f32>(-4.05099170129373878e-05), static_cast<f32>(-4.78285654773798771e-05), 
static_cast<f32>(-4.72436134046799568e-05), static_cast<f32>(-4.06465233808630530e-05), static_cast<f32>(-3.03553393685487895e-05), static_cast<f32>(-1.87026219834234124e-05), static_cast<f32>(-7.69804723413831948e-06), 
static_cast<f32>(1.19871830785515650e-06), static_cast<f32>(7.18107680307455200e-06), static_cast<f32>(1.00811654140955007e-05), 0.0f,
};
    // clang-format on

    // clang-format off
const f32 KaiserBest[257] = {
static_cast<f32>(9.475937167399596239e-01), static_cast<f32>(8.624135863459860829e-01), static_cast<f32>(6.341878970614924782e-01), static_cast<f32>(3.344555394844956897e-01), static_cast<f32>(5.207910617885588844e-02), 
static_cast<f32>(-1.390888908433684235e-01), static_cast<f32>(-2.049638378925058568e-01), static_cast<f32>(-1.590111955193286575e-01), static_cast<f32>(-5.110719159757131508e-02), static_cast<f32>(5.653395433538780518e-02), 
static_cast<f32>(1.154270367012828064e-01), static_cast<f32>(1.079726177927421704e-01), static_cast<f32>(4.951898536900236486e-02), static_cast<f32>(-2.381925994269620817e-02), static_cast<f32>(-7.464952296045440638e-02), 
static_cast<f32>(-8.166212679556161813e-02), static_cast<f32>(-4.736057134080128017e-02), static_cast<f32>(6.210793922854770076e-03), static_cast<f32>(5.038418088331499123e-02), static_cast<f32>(6.442786953978682507e-02), 
static_cast<f32>(4.469375541357477333e-02), static_cast<f32>(4.557869578483456433e-03), static_cast<f32>(-3.388904834641004449e-02), static_cast<f32>(-5.157805950695695113e-02), static_cast<f32>(-4.159352445825559991e-02), 
static_cast<f32>(-1.147461232883913509e-02), static_cast<f32>(2.183464567447396948e-02), static_cast<f32>(4.125927494464539519e-02), static_cast<f32>(3.814501091659006776e-02), static_cast<f32>(1.590124025646893688e-02), 
static_cast<f32>(-1.268413706454708453e-02), static_cast<f32>(-3.261987976693820485e-02), static_cast<f32>(-3.444011722142438503e-02), static_cast<f32>(-1.856829042102058416e-02), static_cast<f32>(5.628139424318459594e-03), 
static_cast<f32>(2.523080156469341417e-02), static_cast<f32>(3.057396828630531277e-02), static_cast<f32>(1.992292659667551161e-02), static_cast<f32>(-1.923769853178876538e-04), static_cast<f32>(-1.886135321949267141e-02), 
static_cast<f32>(-2.664136455366422976e-02), static_cast<f32>(-2.027141059950477928e-02), static_cast<f32>(-3.929885718541407946e-03), static_cast<f32>(1.337934292255188277e-02), static_cast<f32>(2.273340244077692515e-02), 
static_cast<f32>(1.984380005945496697e-02), static_cast<f32>(6.957048357353561203e-03), static_cast<f32>(-8.701862153048892448e-03), static_cast<f32>(-1.893441410935312735e-02), static_cast<f32>(-1.882526786892763093e-02), 
static_cast<f32>(-9.059193761260809705e-03), static_cast<f32>(4.769332470309800044e-03), static_cast<f32>(1.531935551847190581e-02), static_cast<f32>(1.737173338109971807e-02), static_cast<f32>(1.037903537218197100e-02), 
static_cast<f32>(-1.531323796931043958e-03), static_cast<f32>(-1.195174239599720682e-02), static_cast<f32>(-1.561764962937671301e-02), static_cast<f32>(-1.104289317671875402e-02), static_cast<f32>(-1.061153637704711312e-03), 
static_cast<f32>(8.882200126551869496e-03), static_cast<f32>(1.367974368225591611e-02), static_cast<f32>(1.116618260463853735e-02), static_cast<f32>(3.059545247933707305e-03), static_cast<f32>(-6.147657848870015894e-03), 
static_cast<f32>(-1.165868630417580057e-02), static_cast<f32>(-1.085580048601115719e-02), static_cast<f32>(-4.519288794771365685e-03), static_cast<f32>(3.771181562152897335e-03), static_cast<f32>(9.639779233408656348e-03), 
static_cast<f32>(1.021076484601396266e-02), static_cast<f32>(5.499854640548336795e-03), static_cast<f32>(-1.762407917700164030e-03), static_cast<f32>(-7.693281208441664701e-03), static_cast<f32>(-9.321927183047291551e-03), 
static_cast<f32>(-6.063846367385312616e-03), static_cast<f32>(1.185115108734274147e-04), static_cast<f32>(5.874729038284978895e-03), static_cast<f32>(8.271273333990301554e-03), static_cast<f32>(6.275548592271000189e-03), 
static_cast<f32>(1.174384600966857112e-03), static_cast<f32>(-4.225448917118538802e-03), static_cast<f32>(-7.131145607786639785e-03), static_cast<f32>(-6.199205727128644788e-03), static_cast<f32>(-2.139461703133789961e-03), 
static_cast<f32>(2.773349193346273493e-03), static_cast<f32>(5.963598743393953676e-03), static_cast<f32>(5.897245765669923041e-03), static_cast<f32>(2.807237026821082001e-03), static_cast<f32>(-1.534016009767162365e-03), 
static_cast<f32>(-4.820017878079761904e-03), static_cast<f32>(-5.428610459015706900e-03), static_cast<f32>(-3.213491192855966617e-03), static_cast<f32>(5.120859653262437592e-04), static_cast<f32>(3.741064294189949419e-03), 
static_cast<f32>(4.847309499178789616e-03), static_cast<f32>(3.397203213383940407e-03), static_cast<f32>(2.971610352769626083e-04), static_cast<f32>(-2.756967297491399928e-03), static_cast<f32>(-4.201277983869131788e-03), 
static_cast<f32>(-3.398593443259232370e-03), static_cast<f32>(-9.060639163816152589e-04), static_cast<f32>(1.888144654084843816e-03), static_cast<f32>(3.531582195182417228e-03), static_cast<f32>(3.257358763100887730e-03), 
static_cast<f32>(1.332780841988472856e-03), static_cast<f32>(-1.146107733022045782e-03), static_cast<f32>(-2.871988445829827281e-03), static_cast<f32>(-3.011164816146875544e-03), static_cast<f32>(-1.599511003078302879e-03), 
static_cast<f32>(5.345897629122222479e-04), static_cast<f32>(2.248883790300547792e-03), static_cast<f32>(2.694438845065488027e-03), static_cast<f32>(1.730814102261153111e-03), static_cast<f32>(-5.082555095133911186e-05), 
static_cast<f32>(-1.681516249107884798e-03), static_cast<f32>(-2.337485100262779673e-03), static_cast<f32>(-1.752092147026578211e-03), static_cast<f32>(-3.130922358275620176e-04), static_cast<f32>(1.182506237542255015e-03), 
static_cast<f32>(1.965924313408897329e-03), static_cast<f32>(1.688283148779055973e-03), static_cast<f32>(5.688525439344847010e-04), static_cast<f32>(-7.585702905004003968e-04), static_cast<f32>(-1.600440534472678204e-03), 
static_cast<f32>(-1.562799562392161619e-03), static_cast<f32>(-7.306261733328603880e-04), static_cast<f32>(4.113928221087177523e-04), static_cast<f32>(1.256803628554946286e-03), static_cast<f32>(1.396727222401605547e-03), 
static_cast<f32>(8.138809794940206213e-04), static_cast<f32>(-1.385811653966457677e-04), static_cast<f32>(-9.461245184142249697e-04), static_cast<f32>(-1.208284384754270985e-03), static_cast<f32>(-8.343531565651654807e-04), 
static_cast<f32>(-6.535713012515974329e-05), static_cast<f32>(6.752931137441025569e-04), static_cast<f32>(1.012526278092916888e-03), static_cast<f32>(8.072016047844744659e-04), static_cast<f32>(2.080686293537052731e-04), 
static_cast<f32>(-4.475457529016986535e-04), static_cast<f32>(-8.212690439885122822e-04), static_cast<f32>(-7.463583517116818312e-04), static_cast<f32>(-2.984842605659872002e-04), static_cast<f32>(2.631095869473945277e-04), 
static_cast<f32>(6.431985507329121728e-04), static_cast<f32>(6.640750067460789293e-04), static_cast<f32>(3.460587004156766289e-04), static_cast<f32>(-1.198751289491966981e-04), static_cast<f32>(-4.841244659303321871e-04), 
static_cast<f32>(-5.706540373621950182e-04), static_cast<f32>(-3.601405233130335905e-04), static_cast<f32>(1.405448667379323292e-05), static_cast<f32>(3.473380754961624228e-04), static_cast<f32>(4.743447866081208406e-04), 
static_cast<f32>(3.494834999475533418e-04), static_cast<f32>(5.920917985859432164e-05), static_cast<f32>(-2.340333282452700223e-04), static_cast<f32>(-3.813778879063339246e-04), static_cast<f32>(-3.218992523142501162e-04), 
static_cast<f32>(-1.053064758050925890e-04), static_cast<f32>(1.437539866165487581e-04), static_cast<f32>(2.961081294835794082e-04), static_cast<f32>(2.840429038110876868e-04), static_cast<f32>(1.297172201102921398e-04), 
static_cast<f32>(-7.483498724662225729e-05), static_cast<f32>(-2.212347293496766312e-04), static_cast<f32>(-2.413167508071564825e-04), static_cast<f32>(-1.376642099066444633e-04), static_cast<f32>(2.481252283142463069e-05), 
static_cast<f32>(1.580690922334777664e-04), static_cast<f32>(1.978724806094157417e-04), static_cast<f32>(1.338653124996644849e-04), static_cast<f32>(9.215681979837423866e-06), static_cast<f32>(-1.068230055118902507e-04), 
static_cast<f32>(-1.566900455961737473e-04), static_cast<f32>(-1.223775969917646278e-04), static_cast<f32>(-3.029149006356039240e-05), static_cast<f32>(6.689439837130584619e-05), static_cast<f32>(1.197107991117647604e-04), 
static_cast<f32>(1.065225058761126411e-04), static_cast<f32>(4.136402784697268550e-05), static_cast<f32>(-3.713272495131588635e-05), static_cast<f32>(-8.800361505268712971e-05), static_cast<f32>(-8.887800554833402103e-05), 
static_cast<f32>(-4.512353122107880509e-05), static_cast<f32>(1.607124995522908728e-05), static_cast<f32>(6.194508435295652217e-05), static_cast<f32>(7.132217147490020300e-05), static_cast<f32>(4.389655097880094909e-05), 
static_cast<f32>(-2.118585077070912648e-06), static_cast<f32>(-4.139810756218419515e-05), static_cast<f32>(-5.511257857295570264e-05), static_cast<f32>(-3.959452493344489796e-05), static_cast<f32>(-6.293598630425989474e-06), 
static_cast<f32>(2.587688584496377977e-05), static_cast<f32>(4.098692354497868260e-05), static_cast<f32>(3.370578388317479812e-05), static_cast<f32>(1.060592075460525509e-05), static_cast<f32>(-1.469008964334372257e-05), 
static_cast<f32>(-2.927220340694936527e-05), static_cast<f32>(-2.732026535490722255e-05), static_cast<f32>(-1.206599098094592242e-05), static_cast<f32>(7.057549567545783669e-06), static_cast<f32>(1.999220138961530554e-05), 
static_cast<f32>(2.117639389108467470e-05), static_cast<f32>(1.169672773642724934e-05), static_cast<f32>(-2.198935626740422506e-06), static_cast<f32>(-1.296569174653531268e-05), static_cast<f32>(-1.572053429515009009e-05), 
static_cast<f32>(-1.029144676405795576e-05), static_cast<f32>(-6.045838193251553693e-07), static_cast<f32>(7.890410373941007991e-06), static_cast<f32>(1.117089904877507156e-05), static_cast<f32>(8.428548180715721380e-06), 
static_cast<f32>(1.972857252700100825e-06), static_cast<f32>(-4.410242733681185211e-06), static_cast<f32>(-7.579560476830104587e-06), static_cast<f32>(-6.498958003643935799e-06), static_cast<f32>(-2.409412917725605183e-06), 
static_cast<f32>(2.165109006770163950e-06), static_cast<f32>(4.888071947028446579e-06), static_cast<f32>(4.740285523554878853e-06), static_cast<f32>(2.299683716825349741e-06), static_cast<f32>(-8.245949261514169093e-07), 
static_cast<f32>(-2.973966532379414817e-06), static_cast<f32>(-3.272764622651508543e-06), static_cast<f32>(-1.920174567174922909e-06), static_cast<f32>(1.074586433174825847e-07), static_cast<f32>(1.686946733714645200e-06), 
static_cast<f32>(2.133290083890616283e-06), static_cast<f32>(1.454323911983146074e-06), static_cast<f32>(2.102392238104282554e-07), static_cast<f32>(-8.748228640743265134e-07), static_cast<f32>(-1.305097981420971763e-06), 
static_cast<f32>(-1.011457266643626628e-06), static_cast<f32>(-2.954254631637524315e-07), static_cast<f32>(4.001634203526068684e-07), static_cast<f32>(7.417631285990475454e-07), static_cast<f32>(6.460262123094813200e-07), 
static_cast<f32>(2.638826359977218364e-07), static_cast<f32>(-1.491902529050612046e-07), static_cast<f32>(-3.851223477532645345e-07), static_cast<f32>(-3.751605580127972301e-07), static_cast<f32>(-1.892022364936786593e-07), 
static_cast<f32>(3.471732992546920545e-08), static_cast<f32>(1.774408093765072857e-07), static_cast<f32>(1.933461530163775297e-07), static_cast<f32>(1.128159505946651450e-07), static_cast<f32>(5.051910951393845730e-09), 
static_cast<f32>(-6.861160281542139120e-08), static_cast<f32>(-8.371357096328895344e-08), static_cast<f32>(-5.364061067205831902e-08), static_cast<f32>(-1.022235215092043802e-08), static_cast<f32>(1.943267790374893566e-08), 
static_cast<f32>(2.594732810196161353e-08), static_cast<f32>(1.640118306934728671e-08), 
};
    // clang-format on

    // clang-format off
const f32 KaiserBestDiff[257] = {
static_cast<f32>(-8.51801303939735410e-02), static_cast<f32>(-2.28225689284493605e-01), static_cast<f32>(-2.99732357576996788e-01), static_cast<f32>(-2.82376433305639774e-01), static_cast<f32>(-1.91167997022224312e-01), 
static_cast<f32>(-6.58749470491374334e-02), static_cast<f32>(4.59526423731771994e-02), static_cast<f32>(1.07904003921757335e-01), static_cast<f32>(1.07641145932959120e-01), static_cast<f32>(5.88930823658950012e-02), 
static_cast<f32>(-7.45441890854063594e-03), static_cast<f32>(-5.84536324237398056e-02), static_cast<f32>(-7.33382453116985800e-02), static_cast<f32>(-5.08302630177581982e-02), static_cast<f32>(-7.01260383510721175e-03), 
static_cast<f32>(3.43015554547603380e-02), static_cast<f32>(5.35713652636560511e-02), static_cast<f32>(4.41733869604602203e-02), static_cast<f32>(1.40436886564718338e-02), static_cast<f32>(-1.97341141262120517e-02), 
static_cast<f32>(-4.01358858350913186e-02), static_cast<f32>(-3.84469179248934992e-02), static_cast<f32>(-1.76890111605469066e-02), static_cast<f32>(9.98453504870135122e-03), static_cast<f32>(3.01189121294164648e-02), 
static_cast<f32>(3.33092580033131080e-02), static_cast<f32>(1.94246292701714257e-02), static_cast<f32>(-3.11426402805532743e-03), static_cast<f32>(-2.22437706601211309e-02), static_cast<f32>(-2.85853773210160214e-02), 
static_cast<f32>(-1.99357427023911203e-02), static_cast<f32>(-1.82023745448618018e-03), static_cast<f32>(1.58718268004038009e-02), static_cast<f32>(2.41964298453390438e-02), static_cast<f32>(1.96026621403749546e-02), 
static_cast<f32>(5.34316672161189860e-03), static_cast<f32>(-1.06510416896298012e-02), static_cast<f32>(-2.01153035819933990e-02), static_cast<f32>(-1.86689762341747841e-02), static_cast<f32>(-7.78001133417155835e-03), 
static_cast<f32>(6.36995395415945048e-03), static_cast<f32>(1.63415248809633713e-02), static_cast<f32>(1.73092286410932925e-02), static_cast<f32>(9.35405951822504238e-03), static_cast<f32>(-2.88960238132195818e-03), 
static_cast<f32>(-1.28867517021014066e-02), static_cast<f32>(-1.56589105104024545e-02), static_cast<f32>(-1.02325519563042349e-02), static_cast<f32>(1.09146240425496421e-04), static_cast<f32>(9.76607410766682123e-03), 
static_cast<f32>(1.38285262315706106e-02), static_cast<f32>(1.05500230481621049e-02), static_cast<f32>(2.05237786262781226e-03), static_cast<f32>(-6.99269800891774707e-03), static_cast<f32>(-1.19103591691130141e-02), 
static_cast<f32>(-1.04204185990661620e-02), static_cast<f32>(-3.66590723337950619e-03), static_cast<f32>(4.57475645265795899e-03), static_cast<f32>(9.98173953901404336e-03), static_cast<f32>(9.94335376425658016e-03), 
static_cast<f32>(4.79754355570404661e-03), static_cast<f32>(-2.51356107761737876e-03), static_cast<f32>(-8.10663735670483047e-03), static_cast<f32>(-9.20720309680372363e-03), static_cast<f32>(-5.51102845530578467e-03), 
static_cast<f32>(8.02885818164643383e-04), static_cast<f32>(6.33651169123979150e-03), static_cast<f32>(8.29047035692426215e-03), static_cast<f32>(5.86859767125575901e-03), static_cast<f32>(5.70985612605306309e-04), 
static_cast<f32>(-4.71091020546562586e-03), static_cast<f32>(-7.26226255824850061e-03), static_cast<f32>(-5.93087329074150089e-03), static_cast<f32>(-1.62864597460562685e-03), static_cast<f32>(3.25808081566197894e-03), 
static_cast<f32>(6.18235787825874034e-03), static_cast<f32>(5.75621752741155117e-03), static_cast<f32>(2.39654429570532266e-03), static_cast<f32>(-1.99572474171930136e-03), static_cast<f32>(-5.10116399130414351e-03), 
static_cast<f32>(-5.39983351808539548e-03), static_cast<f32>(-2.90569669066810098e-03), static_cast<f32>(9.31939880657994997e-04), static_cast<f32>(4.05974402399485526e-03), static_cast<f32>(4.91281089648006389e-03), 
static_cast<f32>(3.19024955004768018e-03), static_cast<f32>(-6.63529777240306345e-05), static_cast<f32>(-3.09000873884884104e-03), static_cast<f32>(-4.34125303658824437e-03), static_cast<f32>(-3.28600186831259954e-03), 
static_cast<f32>(-6.08592580935944996e-04), static_cast<f32>(2.21511926615974028e-03), static_cast<f32>(3.72557715818221027e-03), static_cast<f32>(3.22897832886370577e-03), static_cast<f32>(1.10624520498884020e-03), 
static_cast<f32>(-1.45010628579484921e-03), static_cast<f32>(-3.10004217810697785e-03), static_cast<f32>(-3.05412833276836248e-03), static_cast<f32>(-1.44431068637773186e-03), static_cast<f32>(8.02684540609899418e-04), 
static_cast<f32>(2.49252952687761733e-03), static_cast<f32>(2.79420857046645908e-03), static_cast<f32>(1.64343754109757341e-03), static_cast<f32>(-2.74223432081529498e-04), static_cast<f32>(-1.92457792111241487e-03), 
static_cast<f32>(-2.47888857501051864e-03), static_cast<f32>(-1.72588071280778150e-03), static_cast<f32>(-1.39176370317048263e-04), static_cast<f32>(1.41165381306857267e-03), static_cast<f32>(2.13410076599052523e-03), 
static_cast<f32>(1.71429402738832544e-03), static_cast<f32>(4.45555054764940235e-04), static_cast<f32>(-9.63624742804334916e-04), static_cast<f32>(-1.78163965321249214e-03), static_cast<f32>(-1.63069069815654577e-03), 
static_cast<f32>(-6.55968851154894875e-04), static_cast<f32>(5.85392953236201462e-04), static_cast<f32>(1.43899991119901625e-03), static_cast<f32>(1.49559847336981698e-03), static_cast<f32>(7.83418075866642314e-04), 
static_cast<f32>(-2.77641164629841355e-04), static_cast<f32>(-1.11943060484457138e-03), static_cast<f32>(-1.32742283443488510e-03), static_cast<f32>(-8.41870243972277807e-04), static_cast<f32>(3.76409720805165847e-05), 
static_cast<f32>(8.32173389059301231e-04), static_cast<f32>(1.14201899544157819e-03), static_cast<f32>(8.45410806446228588e-04), static_cast<f32>(1.39923593846659261e-04), static_cast<f32>(-5.82846242907584925e-04), 
static_cast<f32>(-9.52462144890666362e-04), static_cast<f32>(-8.07543353017579229e-04), static_cast<f32>(-2.62159866340046016e-04), static_cast<f32>(3.73931228189105505e-04), static_cast<f32>(7.68996026440005765e-04), 
static_cast<f32>(7.40650243869262273e-04), static_cast<f32>(3.37233164348814331e-04), static_cast<f32>(-2.05324673308442422e-04), static_cast<f32>(-5.99132975430769193e-04), static_cast<f32>(-6.55614382255403872e-04), 
static_cast<f32>(-3.73723291086813629e-04), static_cast<f32>(7.49106922768304510e-05), static_cast<f32>(4.47874091145694631e-04), static_cast<f32>(5.61593847513381728e-04), static_cast<f32>(3.80088963785517645e-04), 
static_cast<f32>(2.08764560131667566e-05), static_cast<f32>(-3.18016306330402300e-04), static_cast<f32>(-4.65933829364873300e-04), static_cast<f32>(-3.64249336981135462e-04), static_cast<f32>(-8.65295714318628311e-05), 
static_cast<f32>(2.10513514049161428e-04), static_cast<f32>(3.74195009986826813e-04), static_cast<f32>(3.33283588822369200e-04), static_cast<f32>(1.27006711111958418e-04), static_cast<f32>(-1.24861286660567499e-04), 
static_cast<f32>(-2.90274320088959007e-04), static_cast<f32>(-2.93242508103864357e-04), static_cast<f32>(-1.47344559661063902e-04), static_cast<f32>(5.94786355920838084e-05), static_cast<f32>(2.16592776509157527e-04), 
static_cast<f32>(2.49060462421641347e-04), static_cast<f32>(1.52354142867030650e-04), static_cast<f32>(-1.20652256724917213e-05), static_cast<f32>(-1.54325683700795547e-04), static_cast<f32>(-2.04552207356914397e-04), 
static_cast<f32>(-1.46399742103054374e-04), static_cast<f32>(-2.00820214574798513e-05), static_cast<f32>(1.03652540900512019e-04), static_cast<f32>(1.62476732738069108e-04), static_cast<f32>(1.33256569402053122e-04), 
static_cast<f32>(3.98033883759379753e-05), static_cast<f32>(-6.40071681097512568e-05), static_cast<f32>(-1.24649630519827056e-04), static_cast<f32>(-1.16038687491727680e-04), static_cast<f32>(-4.98670400842834966e-05), 
static_cast<f32>(3.43124486044091195e-05), static_cast<f32>(9.20861069282042320e-05), static_cast<f32>(9.71858884348662420e-05), static_cast<f32>(5.28164007404589142e-05), static_cast<f32>(-1.31882932356521194e-05), 
static_cast<f32>(-6.51584780291399556e-05), static_cast<f32>(-7.84967527982885786e-05), static_cast<f32>(-5.08708901013712434e-05), static_cast<f32>(-8.74390495646891323e-07), static_cast<f32>(4.37544743272552159e-05), 
static_cast<f32>(6.11947811763078958e-05), static_cast<f32>(4.58738343977274383e-05), static_cast<f32>(9.37708712194368083e-06), static_cast<f32>(-2.74256204960992539e-05), static_cast<f32>(-4.60151360558718630e-05), 
static_cast<f32>(-3.92795224851132812e-05), static_cast<f32>(-1.37144710107715075e-05), static_cast<f32>(1.55180536395108047e-05), static_cast<f32>(3.33009263030189110e-05), static_cast<f32>(3.21704844753897701e-05), 
static_cast<f32>(1.51100377000149028e-05), static_cast<f32>(-7.28113966180388448e-06), static_cast<f32>(-2.30998631285695430e-05), static_cast<f32>(-2.52960103979489777e-05), static_cast<f32>(-1.45821137636056427e-05), 
static_cast<f32>(1.95193805204214272e-06), static_cast<f32>(1.52542743739613001e-05), static_cast<f32>(1.91235405484917061e-05), static_cast<f32>(1.29346518220695219e-05), static_cast<f32>(1.18419250146936916e-06), 
static_cast<f32>(-9.47966615465742536e-06), static_cast<f32>(-1.38956633631676718e-05), static_cast<f32>(-1.07667561197948902e-05), static_cast<f32>(-2.75484254861477741e-06), static_cast<f32>(5.42908753109213433e-06), 
static_cast<f32>(9.68686294473280018e-06), static_cast<f32>(8.49499419326616357e-06), static_cast<f32>(3.28048867483406357e-06), static_cast<f32>(-2.74235086805935018e-06), static_cast<f32>(-6.45569092801562013e-06), 
static_cast<f32>(-6.38309998638128561e-06), static_cast<f32>(-3.16931774314891938e-06), static_cast<f32>(1.08060247318616879e-06), static_cast<f32>(4.08954508591833062e-06), static_cast<f32>(4.57452192449576956e-06), 
static_cast<f32>(2.72296294025828263e-06), static_cast<f32>(-1.47786423473567726e-07), static_cast<f32>(-2.44060180672952911e-06), static_cast<f32>(-3.12427864297676676e-06), static_cast<f32>(-2.14937160622799780e-06), 
static_cast<f32>(-2.98798090272093726e-07), static_cast<f32>(1.35259005547658563e-06), static_cast<f32>(2.02763321049240551e-06), static_cast<f32>(1.57948809039716260e-06), static_cast<f32>(4.46343350175971083e-07), 
static_cast<f32>(-6.78966171907470209e-07), static_cast<f32>(-1.24408468817271787e-06), static_cast<f32>(-1.08506208788475472e-06), static_cast<f32>(-4.30275117346645250e-07), static_cast<f32>(2.93640714777345135e-07), 
static_cast<f32>(7.16031803479874197e-07), static_cast<f32>(6.95588883516359300e-07), static_cast<f32>(3.41599708246440677e-07), static_cast<f32>(-9.57369162895662254e-08), static_cast<f32>(-3.82143576311759484e-07), 
static_cast<f32>(-4.13072888902783041e-07), static_cast<f32>(-2.35932094848203330e-07), static_cast<f32>(9.96178974046730438e-09), static_cast<f32>(1.85958321519118571e-07), static_cast<f32>(2.23919566419147871e-07), 
static_cast<f32>(1.42723479451038074e-07), static_cast<f32>(1.59053436398702440e-08), static_cast<f32>(-8.05302024217123848e-08), static_cast<f32>(-1.07764039643271304e-07), static_cast<f32>(-7.36635137668152320e-08), 
static_cast<f32>(-1.51019681478675622e-08), static_cast<f32>(3.00729602912306344e-08), static_cast<f32>(4.34182585211378827e-08), static_cast<f32>(2.96550300546693720e-08), static_cast<f32>(6.51465019821267787e-09), 
static_cast<f32>(-9.54614503261432682e-09), 0.0f,
};
    // clang-format on
    struct Config
    {
        u32 oversample_;
        u32 taps_;
    };

    Config Configs[] = {
        {8, 129},
        {4, 257},
    };
} // namespace

Resampler Resampler::initialize(u32 srcFrequency, u32 dstFrequency, Quality quality)
{
    Resampler resampler;
    resampler.srcFrequency_ = srcFrequency;
    resampler.dstFrequency_ = dstFrequency;
    resampler.sampleRatio_ = static_cast<f32>(dstFrequency) / srcFrequency;
    resampler.quality_ = static_cast<u32>(quality);
    return resampler;
}

u32 Resampler::run(u32 channels, u32 dstSamples, s16* dst, u32 srcSamples, const s16* src)
{
    u32 oversample;
    u32 taps;
    const f32* filter;
    const f32* filterDelta;
    switch(quality_) {
    case static_cast<u32>(Quality::Fast):
        oversample = Configs[static_cast<u32>(Quality::Fast)].oversample_;
        taps = Configs[static_cast<u32>(Quality::Fast)].taps_;
        filter = KaiserFast;
        filterDelta = KaiserFastDiff;
        break;
    case static_cast<u32>(Quality::Best):
        oversample = Configs[static_cast<u32>(Quality::Best)].oversample_;
        taps = Configs[static_cast<u32>(Quality::Best)].taps_;
        filter = KaiserBest;
        filterDelta = KaiserBestDiff;
        break;
    default:
        RESAMCPP_ASSERT(false);
        return 0;
    }
    f32 sampleRatio = sampleRatio_;
    f32 scale = minimum(1.0f, sampleRatio);
    f32 timeStep = 1.0f / sampleRatio;
    u32 indexStep = static_cast<u32>(scale * oversample);

    f32 time = 0.0f;
    for(u32 i = 0; i < dstSamples; ++i) {
        // Grab the top bits as an index to the input buffer
        u32 n = static_cast<u32>(time);

        // Grab the fractional component ot the time index
        f32 frac = scale * (time - n);

        // Offset into the filter
        f32 indexFrac = frac * oversample;
        u32 offset = static_cast<u32>(indexFrac);

        // Interpolation factor
        f32 eta = indexFrac - offset;

        // Compute the left wing of the filter response
        f32 values[2] = {};
        u32 maxi = minimum(n + 1, (taps - offset) / indexStep);
        for(u32 j = 0; j < maxi; ++j) {
            RESAMCPP_ASSERT((offset + j * indexStep) < taps);
            f32 weight = (filter[offset + j * indexStep] + eta * filterDelta[offset + j * indexStep]);
            for(u32 k = 0; k < channels; ++k) {
                RESAMCPP_ASSERT((i * channels + k) < (dstSamples*channels));
                RESAMCPP_ASSERT(((n - j) * channels + k) < (srcSamples*channels));
                values[k] += weight * src[(n - j) * channels + k];
            }
        }
        // Invert P
        frac = scale - frac;
        indexFrac = frac * oversample;
        offset = static_cast<u32>(indexFrac);

        // Offset into the filter
        eta = indexFrac - offset;

        // Compute the right wing of the filter response
        maxi = minimum(srcSamples - n - 1, (taps - offset) / indexStep);
        for(u32 j = 0; j < maxi; ++j) {
            RESAMCPP_ASSERT((offset + j * indexStep) < taps);
            f32 weight = (filter[offset + j * indexStep] + eta * filterDelta[offset + j * indexStep]);
            for(u32 k = 0; k < channels; ++k) {
                RESAMCPP_ASSERT((i * channels + k) < (dstSamples*channels));
                RESAMCPP_ASSERT(((n + j + 1) * channels + k) < (srcSamples*channels));
                values[k] += weight * src[(n + j + 1) * channels + k];
            }
        }
        for(u32 j = 0; j < channels; ++j) {
            s32 x = (s32)(values[j] * scale);
            dst[i * channels + j] = static_cast<s16>(clamp(x, -32768, 32767));
        }
        // Increment the time register
        time += timeStep;
    }
    return dstSamples;
}

u32 Resampler::run_ispc(u32 channels, u32 dstSamples, s16* dst, u32 srcSamples, const s16* src)
{
#ifdef RESAMCPP_ISPC
    u32 oversample;
    s32 taps;
    const f32* filter;
    const f32* filterDelta;
    switch(quality_) {
    case static_cast<u32>(Quality::Fast):
        oversample = Configs[static_cast<u32>(Quality::Fast)].oversample_;
        taps = static_cast<s32>(Configs[static_cast<s32>(Quality::Fast)].taps_);
        filter = KaiserFast;
        filterDelta = KaiserFastDiff;
        break;
    case static_cast<u32>(Quality::Best):
        oversample = Configs[static_cast<u32>(Quality::Best)].oversample_;
        taps = static_cast<s32>(Configs[static_cast<s32>(Quality::Best)].taps_);
        filter = KaiserBest;
        filterDelta = KaiserBestDiff;
        break;
    default:
        RESAMCPP_ASSERT(false);
        return 0;
    }
    return ispc::resample(
        channels,
        sampleRatio_,
        oversample,
        dstSamples,
        dst,
        srcSamples,
        src,
        filter,
        filterDelta);
#else
    return run(channels, dstSamples, dst, srcSamples, src);
#endif
}
} // namespace resamcpp
