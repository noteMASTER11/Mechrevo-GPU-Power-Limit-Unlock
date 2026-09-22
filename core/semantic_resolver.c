/* SPDX-License-Identifier: MIT
 * Address-free, offset-free semantic resolver reference core.
 */
#include "semantic_core.h"

#define POLICY_SLOTS 20U
#define PREFIX_LIMIT 0x1000U
#define IDENTITY_LIMIT 0x80U
#define CANDIDATE_LIMIT 64U
#define BOARD_CONTRACT_FIRST 0x300U
#define BOARD_CONTRACT_LAST  0x500U
#define RAIL_CONTRACT_LAST   0x300U
#define BASE_STOCK_MW 150000U
#define BASE_TARGET_MW 225000U
#define ENTRY13_STOCK 210000U
#define ENTRY13_TARGET 250000U
#define ENTRY14_STOCK 60000U
#define ENTRY14_TARGET 100000U

static int rd(const struct gs_io *io, uint64_t o, void *p, uint32_t n)
{
    if (n > io->heap_size || o > io->heap_size - n) return GS_INVALID;
    return io->read(io->context, o, p, n) ? GS_IO : GS_OK;
}
static int r8(const struct gs_io *io, uint64_t o, uint8_t *v) { return rd(io,o,v,1); }
static int r16(const struct gs_io *io, uint64_t o, uint16_t *v) { return rd(io,o,v,2); }
static int r32(const struct gs_io *io, uint64_t o, uint32_t *v) { return rd(io,o,v,4); }
static int r64(const struct gs_io *io, uint64_t o, uint64_t *v) { return rd(io,o,v,8); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static uint64_t le64(const uint8_t *p) { return (uint64_t)le32(p)|((uint64_t)le32(p+4)<<32); }

static int same(const struct gs_resolution *a, const struct gs_resolution *b)
{
    uint32_t i;
    if(!(a->va_base==b->va_base&&a->policy_array==b->policy_array&&
        a->board_object==b->board_object&&a->selector==b->selector&&
        a->lower_selector==b->lower_selector&&a->ctgp_tuple==b->ctgp_tuple&&
        a->index_member==b->index_member&&a->type_member==b->type_member&&
        a->id_member==b->id_member&&a->unit_member==b->unit_member&&
        a->board_max_effective==b->board_max_effective&&
        a->board_max_source==b->board_max_source&&a->pmgr_upper==b->pmgr_upper&&
        a->stock_upper_mw==b->stock_upper_mw&&a->current_upper_mw==b->current_upper_mw&&
        a->base_internal==b->base_internal&&a->entry13_object==b->entry13_object&&
        a->entry14_object==b->entry14_object&&a->stock_base_mw==b->stock_base_mw&&
        a->current_base_mw==b->current_base_mw&&a->stock_entry13==b->stock_entry13&&
        a->current_entry13==b->current_entry13&&a->stock_entry14==b->stock_entry14&&
        a->current_entry14==b->current_entry14&&a->current_selector==b->current_selector&&
        a->current_member==b->current_member&&a->stock_current_mw==b->stock_current_mw&&
        a->current_current_mw==b->current_current_mw))return 0;
    for(i=0;i<GS_RAIL_WRITERS;i++)if(a->entry13_writers[i]!=b->entry13_writers[i]||
                                      a->entry14_writers[i]!=b->entry14_writers[i])return 0;
    return 1;
}

static int find_current_contract(const struct gs_io *io, uint64_t board,
                                 uint32_t lower, uint64_t *selector,
                                 uint64_t *member, uint32_t *current)
{
    uint32_t hits=0;int st,stock_shape,active_shape;
    for(uint32_t rel=0;rel+20<=RAIL_CONTRACT_LAST;rel+=4){
        uint8_t bytes[20];uint32_t effective,reference,source_value;
        if((st=rd(io,board+rel,bytes,sizeof(bytes))))return st;
        effective=le32(bytes+4);reference=le32(bytes+8);source_value=le32(bytes+16);
        stock_shape=bytes[1]==2&&effective==lower&&source_value==lower;
        active_shape=bytes[1]==3&&effective==BASE_TARGET_MW&&source_value==0xffffffffU;
        if(bytes[0]!=0||bytes[12]!=0xfe||reference!=lower||
           (!stock_shape&&!active_shape))continue;
        *selector=board+rel;*member=board+rel+4;*current=effective;hits++;
    }
    return hits==1?GS_OK:(hits?GS_AMBIGUOUS:GS_NOT_FOUND);
}

static int find_base_internal(const struct gs_io *io, uint64_t board,
                              uint64_t *member, uint32_t *current)
{
    uint32_t hits=0, value;int st;
    for(uint32_t rel=BOARD_CONTRACT_FIRST;rel<=BOARD_CONTRACT_LAST;rel+=4){
        if((st=r32(io,board+rel,&value)))return st;
        if(value==BASE_STOCK_MW||value==BASE_TARGET_MW){*member=board+rel;*current=value;hits++;}
    }
    return hits==1?GS_OK:(hits?GS_AMBIGUOUS:GS_NOT_FOUND);
}

static int find_rail_contract(const struct gs_io *io, uint64_t object,
                              uint16_t index, uint8_t id, uint32_t stock,
                              uint32_t target, uint32_t index_member,
                              uint64_t writers[GS_RAIL_WRITERS],uint32_t *current)
{
    uint16_t observed_index;uint32_t count=0,state=0;int st;
    uint32_t type_hits=0,id_hits=0;
    if((st=r16(io,object+index_member,&observed_index)))return st;
    if(observed_index!=index)return GS_NOT_FOUND;
    /* The type and ID positions are discovered by their cross-entry relation,
     * rather than assumed from a driver-version layout. */
    for(uint32_t rel=0;rel<IDENTITY_LIMIT;rel++){
        uint8_t a;
        if((st=r8(io,object+rel,&a)))return st;
        if(a==18)type_hits++;
        if(a==id)id_hits++;
    }
    if(type_hits!=1||id_hits!=1)return GS_NOT_FOUND;
    for(uint32_t rel=0;rel+20<=RAIL_CONTRACT_LAST;rel+=4){
        uint8_t bytes[20];uint32_t effective,reference,source_value;
        if((st=rd(io,object+rel,bytes,sizeof(bytes))))return st;
        effective=le32(bytes+4);reference=le32(bytes+8);source_value=le32(bytes+16);
        if(bytes[0]!=0||bytes[1]!=1||bytes[12]!=0xfe||reference!=stock||
           effective!=source_value||(effective!=stock&&effective!=target))continue;
        if(count>=3)return GS_AMBIGUOUS;
        writers[count*2]=object+rel+4;writers[count*2+1]=object+rel+16;
        if(!count)state=effective;else if(state!=effective)return GS_INVALID;
        count++;
    }
    if(count!=3)return GS_NOT_FOUND;
    *current=state;return GS_OK;
}

static int enrich_contract(const struct gs_io *io, const uint64_t objects[POLICY_SLOTS],
                           struct gs_resolution *candidate)
{
    int st;uint32_t lower;
    if(!objects[13]||!objects[14])return GS_NOT_FOUND;
    candidate->entry13_object=objects[13];candidate->entry14_object=objects[14];
    candidate->stock_base_mw=BASE_STOCK_MW;
    candidate->stock_entry13=ENTRY13_STOCK;candidate->stock_entry14=ENTRY14_STOCK;
    st=find_base_internal(io,candidate->board_object,&candidate->base_internal,
                          &candidate->current_base_mw);if(st)return st;
    st=find_rail_contract(io,objects[13],13,27,ENTRY13_STOCK,ENTRY13_TARGET,
                          candidate->index_member,candidate->entry13_writers,
                          &candidate->current_entry13);if(st)return st;
    st=find_rail_contract(io,objects[14],14,28,ENTRY14_STOCK,ENTRY14_TARGET,
                          candidate->index_member,candidate->entry14_writers,
                          &candidate->current_entry14);if(st)return st;
    if((st=r32(io,candidate->ctgp_tuple+4,&lower)))return st;
    candidate->stock_current_mw=lower;
    st=find_current_contract(io,candidate->board_object,lower,
                             &candidate->current_selector,&candidate->current_member,
                             &candidate->current_current_mw);if(st)return st;
    return GS_OK;
}

struct selector_candidate { uint64_t offset; uint32_t effective,stock; };
struct ctgp_candidate { uint64_t offset; uint32_t lower,upper; };

int gs_resolve_autonomous(const struct gs_io *io, struct gs_resolution *out)
{
    struct selector_candidate selectors[CANDIDATE_LIMIT];
    struct ctgp_candidate ctgps[CANDIDATE_LIMIT];
    uint32_t ns=0,nc=0,solutions=0;struct gs_resolution answer={0};int st;
    if(!io||!io->read||!out||io->heap_size<0x10000||io->heap_size>GS_HEAP_LIMIT)return GS_INVALID;
    for(uint64_t o=0;o+20<=io->heap_size;o+=4){
        uint8_t window[20],mode,count,source;uint32_t first,effective,stock,source_value;
        if((st=rd(io,o,window,sizeof(window))))return st;
        mode=window[0];count=window[1];source=window[12];first=le32(window);
        effective=le32(window+4);stock=le32(window+8);source_value=le32(window+16);
        if(mode==0&&count==1&&source==0xfe&&effective==source_value&&
           stock>=20000&&stock<=effective&&effective<=400000){
            if(ns==CANDIDATE_LIMIT)return GS_BUDGET;
            selectors[ns++]=(struct selector_candidate){o,effective,stock};
        }
        if(first==2&&effective>=20000&&effective<=stock&&stock<=400000){
            if(nc==CANDIDATE_LIMIT)return GS_BUDGET;
            ctgps[nc++]=(struct ctgp_candidate){o,effective,stock};
        }
    }
    if(!ns||!nc)return GS_NOT_FOUND;

    /* A cTGP bound must be independently represented by selector defaults. */
    for(uint32_t c=0;c<nc;c++){
        int lo=0,hi=0;for(uint32_t s=0;s<ns;s++){
            if(selectors[s].stock==ctgps[c].lower||selectors[s].effective==ctgps[c].lower)lo=1;
            if(selectors[s].stock==ctgps[c].upper||selectors[s].effective==ctgps[c].upper)hi=1;
        }
        if(!lo||!hi)ctgps[c].offset=~0ULL;
    }

    /* Candidate arrays are discovered from shape, then accepted only if all
     * non-null entries resolve to heap objects with one common index member. */
    for(uint64_t array=0;array+POLICY_SLOTS*8<=io->heap_size;array+=8){
        uint8_t window[POLICY_SLOTS*8];uint64_t raw[POLICY_SLOTS],lo=~0ULL,hi=0;uint32_t active=0;int shape=1;
        if((st=rd(io,array,window,sizeof(window))))return st;
        for(uint32_t i=0;i<POLICY_SLOTS;i++){
            raw[i]=le64(window+i*8);
            if(raw[i]){if(raw[i]&7){shape=0;break;}if(raw[i]<lo)lo=raw[i];if(raw[i]>hi)hi=raw[i];active++;}
        }
        if(!shape||active<8||!raw[2]||hi-lo>=io->heap_size)continue;
        for(uint32_t s=0;s<ns;s++){
            uint64_t selector=selectors[s].offset,first=selector>PREFIX_LIMIT?selector-PREFIX_LIMIT:0;
            uint64_t board=(first&~0xfffULL)|(raw[2]&0xfffULL);
            if(board<first)board+=0x1000;
            for(;board<=selector;board+=0x1000){
                uint64_t base=raw[2]-board,objects[POLICY_SLOTS]={0};uint32_t im=0,im_count=0;int good=1;
                if(!base||(base&0xfff))continue;
                for(uint32_t i=0;i<POLICY_SLOTS;i++)if(raw[i]){
                    if(raw[i]<base||raw[i]-base>=io->heap_size){good=0;break;}objects[i]=raw[i]-base;
                }
                if(!good||objects[2]!=board)continue;
                for(uint32_t member=0;member<IDENTITY_LIMIT;member++){
                    int match=1;for(uint32_t i=0;i<POLICY_SLOTS;i++)if(raw[i]){
                        uint8_t value;if((st=r8(io,objects[i]+member,&value)))return st;
                        if(value!=i){match=0;break;}
                    }
                    if(match){im=member;im_count++;}
                }
                if(im_count!=1)continue;
                for(uint32_t i=0;i<POLICY_SLOTS;i++)if(raw[i]){
                    uint16_t value;if((st=r16(io,objects[i]+im,&value)))return st;
                    if(value!=i){good=0;break;}
                }
                if(!good||selector<board||selector>=board+PREFIX_LIMIT)continue;
                {
                    uint32_t links=0;uint64_t lower_selector=0,ctgp=0;
                    for(uint32_t c=0;c<nc;c++)if(ctgps[c].offset!=~0ULL&&ctgps[c].upper==selectors[s].effective){
                        uint32_t lower_hits=0;uint64_t lower_at=0;
                        for(uint32_t q=0;q<ns;q++)if((selectors[q].stock==ctgps[c].lower||selectors[q].effective==ctgps[c].lower)&&
                            selectors[q].offset>=board&&selectors[q].offset<board+PREFIX_LIMIT){lower_hits++;lower_at=selectors[q].offset;}
                        if(lower_hits==1){links++;lower_selector=lower_at;ctgp=ctgps[c].offset;}
                    }
                    if(links==1){
                        uint32_t current;if((st=r32(io,ctgp+8,&current)))return st;
                        struct gs_resolution candidate={0};
                        candidate.va_base=base;candidate.policy_array=array;candidate.board_object=board;
                        candidate.selector=selector;candidate.lower_selector=lower_selector;candidate.ctgp_tuple=ctgp;
                        candidate.index_member=im;candidate.type_member=candidate.id_member=candidate.unit_member=~0U;
                        candidate.board_max_effective=selector+4;candidate.board_max_source=selector+16;
                        candidate.pmgr_upper=ctgp+8;candidate.stock_upper_mw=selectors[s].stock;
                        candidate.current_upper_mw=current;
                        st=enrich_contract(io,objects,&candidate);if(st==GS_IO||st==GS_INVALID)return st;
                        if(st!=GS_OK)continue;
                        if(!solutions){answer=candidate;solutions=1;}
                        else if(!same(&answer,&candidate))solutions++;
                    }
                }
            }
        }
    }
    if(!solutions)return GS_NOT_FOUND;
    if(solutions!=1)return GS_AMBIGUOUS;
    *out=answer;
    return GS_OK;
}

int gs_resolve(const struct gs_io *io, uint32_t mask,
               const struct gs_policy *ps, uint32_t n,
               struct gs_resolution *out)
{
    uint64_t selectors[CANDIDATE_LIMIT], arrays[CANDIDATE_LIMIT], ctgps[CANDIDATE_LIMIT];
    uint32_t ns=0, na=0, nc=0, i, j, board_slot=~0U, expected_mask=0;
    uint32_t board_lower=0, board_upper=0, solutions=0;
    struct gs_resolution answer={0};
    int st;
    if (!io || !io->read || !ps || !out || n==0 || n>GS_POLICY_LIMIT ||
        io->heap_size<0x10000 || io->heap_size>GS_HEAP_LIMIT || (mask>>POLICY_SLOTS)) return GS_INVALID;
    for(i=0;i<n;i++) {
        if (ps[i].index>=POLICY_SLOTS || ps[i].lower_mw>ps[i].upper_mw ||
            (expected_mask&(1U<<ps[i].index))) return GS_INVALID;
        expected_mask|=1U<<ps[i].index;
        if(ps[i].index==2){board_slot=i;board_lower=ps[i].lower_mw;board_upper=ps[i].upper_mw;}
    }
    if(expected_mask!=mask || board_slot==~0U)return GS_INVALID;

    /* Pass 1: semantic records. */
    for(uint64_t o=0;o+20<=io->heap_size;o+=4){
        uint8_t mode,count,source;uint32_t first,effective,stock,source_value;
        if((st=r8(io,o,&mode))||(st=r8(io,o+1,&count))||(st=r32(io,o+4,&effective))||
           (st=r32(io,o,&first))||(st=r32(io,o+8,&stock))||(st=r8(io,o+12,&source))||
           (st=r32(io,o+16,&source_value)))return st;
        if(mode==0&&count==1&&stock==board_upper&&source==0xfe&&effective==source_value&&
           effective>=board_upper&&effective<=400000){
            if(ns==CANDIDATE_LIMIT)return GS_BUDGET;
            selectors[ns++]=o;
        }
        if(first==2&&effective==board_lower&&stock>=board_upper&&stock<=400000){
            if(nc==CANDIDATE_LIMIT)return GS_BUDGET;
            ctgps[nc++]=o;
        }
    }
    if(ns==0||nc==0)return GS_NOT_FOUND;

    /* Pass 2: arrays whose null/non-null shape exactly follows public mask. */
    for(uint64_t o=0;o+POLICY_SLOTS*8<=io->heap_size;o+=8){
        uint64_t lo=~0ULL,hi=0,p;int good=1;
        for(i=0;i<POLICY_SLOTS;i++){
            if((st=r64(io,o+i*8,&p)))return st;
            if(mask&(1U<<i)){if(!p||(p&7)){good=0;break;}if(p<lo)lo=p;if(p>hi)hi=p;}
            else if(p){good=0;break;}
        }
        if(good&&hi-lo<io->heap_size){if(na==CANDIDATE_LIMIT)return GS_BUDGET;arrays[na++]=o;}
    }
    if(na==0)return GS_NOT_FOUND;

    for(uint32_t si=0;si<ns;si++)for(uint32_t ai=0;ai<na;ai++){
        uint64_t selector=selectors[si],array=arrays[ai],board_va,first;
        if((st=r64(io,array+16,&board_va)))return st;
        first=selector>PREFIX_LIMIT?selector-PREFIX_LIMIT:0; first=(first+7)&~7ULL;
        for(uint64_t board=first;board<=selector;board+=8){
            uint64_t base=board_va-board,objects[POLICY_SLOTS]={0};int good=1;
            uint32_t im[4]={0},counts[4]={0};
            if(!base||(base&0xfff))continue;
            for(i=0;i<n;i++){
                uint64_t p;if((st=r64(io,array+ps[i].index*8,&p)))return st;
                if(p<base||p-base>=io->heap_size){good=0;break;}objects[ps[i].index]=p-base;
            }
            if(!good||objects[2]!=board)continue;
            for(uint32_t member=0;member<IDENTITY_LIMIT;member++){
                int match[4]={1,1,1,1};
                for(i=0;i<n;i++){
                    uint8_t v;if((st=r8(io,objects[ps[i].index]+member,&v)))return st;
                    if(v!=ps[i].index)match[0]=0;
                    if(v!=ps[i].type)match[1]=0;
                    if(v!=ps[i].id)match[2]=0;
                    if(v!=ps[i].unit)match[3]=0;
                }
                for(j=0;j<4;j++)if(match[j]){im[j]=member;counts[j]++;}
            }
            if(counts[0]!=1||counts[1]!=1||counts[2]!=1||counts[3]!=1||
               im[0]==im[1]||im[0]==im[2]||im[0]==im[3]||im[1]==im[2]||
               im[1]==im[3]||im[2]==im[3])continue;
            for(i=0;i<n;i++){uint16_t v;if((st=r16(io,objects[ps[i].index]+im[0],&v)))return st;if(v!=ps[i].index){good=0;break;}}
            if(!good||selector<board||selector>=board+PREFIX_LIMIT)continue;
            /* A coherent topology must contain exactly one cTGP tuple. */
            if(nc!=1)continue;
            {
                uint32_t current;if((st=r32(io,ctgps[0]+8,&current)))return st;
                struct gs_resolution candidate={0};
                candidate.va_base=base;candidate.policy_array=array;candidate.board_object=board;
                candidate.selector=selector;candidate.ctgp_tuple=ctgps[0];
                candidate.index_member=im[0];candidate.type_member=im[1];
                candidate.id_member=im[2];candidate.unit_member=im[3];
                candidate.board_max_effective=selector+4;candidate.board_max_source=selector+16;
                candidate.pmgr_upper=ctgps[0]+8;candidate.stock_upper_mw=board_upper;
                candidate.current_upper_mw=current;
                st=enrich_contract(io,objects,&candidate);if(st==GS_IO||st==GS_INVALID)return st;
                if(st!=GS_OK)continue;
                if(solutions==0){answer=candidate;solutions=1;}
                else if(!same(&answer,&candidate))solutions++;
            }
        }
    }
    if(solutions==0)return GS_NOT_FOUND;
    if(solutions!=1)return GS_AMBIGUOUS;
    *out=answer;return GS_OK;
}
