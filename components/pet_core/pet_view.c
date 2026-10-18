/* Traduce el estado de la partida a lo que necesita pet_render. */
#include "pet_core.h"

static uint8_t expr_for(pet_mood_t m)
{
    switch (m) {
    case MOOD_ASLEEP: return EXPR_SLEEP;
    case MOOD_HUNGRY: return EXPR_HUNGRY;
    case MOOD_DIRTY:  return EXPR_ANGRY;   /* gruñon: quiere que lo asees */
    case MOOD_SICK:
    case MOOD_TIRED:
    case MOOD_BORED:  return EXPR_SAD;
    case MOOD_HAPPY:  return EXPR_HAPPY;
    default:          return EXPR_NEUTRAL;
    }
}

pet_anim_t pet_view(const pet_state_t *s, int64_t now_s, uint32_t mono_ms, pet_view_t *out)
{
    *out = (pet_view_t){ .seed = s->view_seed, .stage = s->stage };
    out->variant = s->stage == PET_ADULT ? s->variant : VAR_NORMAL;
    out->acc = s->stage == PET_EGG ? ACC_NONE : s->acc_worn;
    if (s->stage == PET_EGG)
        out->shape = (uint8_t)((s->view_seed >> 8) % (uint32_t)pet_shape_count(PET_EGG));
    else if (s->stage == PET_ADULT && s->branch > BRANCH_NONE)
        out->shape = (uint8_t)((s->branch - 1) % pet_shape_count(PET_ADULT));

    if (s->stage == PET_EGG) {
        uint32_t pct = (uint32_t)s->egg_active_s * 100 / PET_EGG_HATCH_S;
        out->crack = pct >= 85 ? 2 : (pct >= 50 ? 1 : 0);
    }
    pet_mood_t m = pet_mood(s, now_s);
    out->expr = s->stage == PET_EGG ? EXPR_NEUTRAL : expr_for(m);
    bool live = (int32_t)(s->react_until_ms - mono_ms) > 0;
    if (live && s->stage != PET_EGG && m != MOOD_ASLEEP) {
        switch (s->react_kind) {
        case REACT_PET:     out->expr = EXPR_HAPPY; break;
        case REACT_DIZZY:   out->expr = EXPR_ANGRY; break;
        case REACT_SHAKE:   out->expr = EXPR_SURPRISE; break;
        case REACT_USB_IN:
        case REACT_LOVE:    out->expr = EXPR_LOVE; break;
        case REACT_USB_OUT: out->expr = EXPR_SAD; break;
        default: break;
        }
    }
    if (m == MOOD_ASLEEP && s->stage != PET_EGG) return ANIM_SLEEP;
    return s->behavior == BEH_PLAY && !live ? ANIM_BOUNCE : ANIM_IDLE;
}
