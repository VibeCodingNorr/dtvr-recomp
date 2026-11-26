// SPDX-License-Identifier: GPL-3.0-only
// Card-shop input from rally.exe seg3:4674, 46A2, 4A41, 4B42 and 5028.
#include "card_shop_view.hpp"
#include "save.hpp"

#include <chrono>
#include <cstdio>
#include <sstream>
#include <stdexcept>

using namespace dtvr;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)
using Kind = CardShopView::Action::Kind;

static bool idle(const CardShopView::Action& action) { return action.kind == Kind::None && action.sound == -1; }

static CardShopView::Action key(CardShopView& view, const Game& game, SDL_Keycode code) {
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = code;
    return view.handle(event, game);
}

static CardShopView::Action click(CardShopView& view, const Game& game, const ScreenLayout& layout,
                                  int x, int y, Uint8 count = 1, Uint8 button = SDL_BUTTON_LEFT) {
    SDL_Event event{};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.x = (layout.width - 558) / 2 + x;
    event.button.y = (layout.height - 462) / 2 + y;
    event.button.clicks = count;
    event.button.button = button;
    return view.handle(event, game);
}

// Generate a tiny synthetic palette BMP; the missing SBS files load no textures.
// Original art and a renderer are unnecessary for input tests.
struct SyntheticAssets {
    fs::path root = fs::temp_directory_path() / ("dtvr-shop-input-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    SyntheticAssets() {
        if (!fs::create_directory(root)) throw std::runtime_error("Cannot create synthetic asset directory");
        fs::create_directory(root / "butt");
        Surface palette(SDL_CreateRGBSurfaceWithFormat(0, 1, 1, 8, SDL_PIXELFORMAT_INDEX8), SDL_FreeSurface);
        if (palette) SDL_FillRect(palette.get(), nullptr, 0);
        if (!palette || SDL_SaveBMP(palette.get(), (root / "butt/ziel.bmp").c_str()) != 0)
            throw std::runtime_error("Cannot create synthetic palette BMP");
    }
    ~SyntheticAssets() { std::error_code error; fs::remove_all(root, error); }
};

static Board boardForShop(std::vector<Town>& towns) {
    Board board;
    board.fields.resize(80);
    for (int i = 0; i < 80; ++i) {
        auto& field = board.fields[size_t(i)];
        field.x = uint8_t(i);
        field.kind = 5;
        field.next[1] = int16_t((i + 1) % 80);
        field.next[3] = int16_t((i + 79) % 80);
    }
    board.fields[0].kind = board.fields[79].kind = 6;
    towns.resize(2);
    towns[0].name = "London";
    towns[1].name = "Rom";
    towns[1].x = 79;
    return board;
}

// Empties hand slot `slot` of `player` through the save codec: a sale in the shop closes
// its gap (seg3:49CC -> seg2:3CF1), cards played elsewhere leave one.
static void openGap(Game& game, int player, int slot) {
    std::istringstream in(SaveCodec::encode(game));
    std::string line, out, error;
    const std::string key = "hand " + std::to_string(player) + ' ';
    while (std::getline(in, line)) {
        if (line.rfind(key, 0) == 0) {
            std::istringstream words(line.substr(key.size()));
            std::string word, rebuilt = key;
            for (int k = 0; words >> word; ++k) rebuilt += (k ? " " : "") + (k == slot ? std::string("255") : word);
            line = rebuilt;
        }
        out += line + '\n';
    }
    CHECK(SaveCodec::decode(game, out, error));
}

static void arrive(Game& game) {
    game.setRandomEvents(false);
    CHECK(game.roll());
    while (game.phase() == Game::Phase::Move) {
        const bool moved = game.step(Direction::Right);
        CHECK(moved);
        if (!moved) break;
    }
    CHECK(game.atCardField());
}

int main() {
    SyntheticAssets assets;
    TextRenderer text;
    const std::map<int, std::string> texts;
    std::vector<Town> towns;
    const Board board = boardForShop(towns);
    Game game(board, towns, 2, 2, 1, 8);
    arrive(game);
    CHECK(game.handCount(0) > 4);
    openGap(game, 0, 2); // a gap tests sell-list indexing versus hand-slot numbers
    CHECK(game.hand(0)[2] == Deck::kTaken && game.hand(0)[3] != Deck::kTaken);
    Game empty(board, towns, 2, 2, 3, 1);
    arrive(empty);
    CHECK(empty.sellCard(0) && empty.handCount(0) == 0);
    const auto offers = cardShopOffers(game.deck());
    CHECK(offers.size() > 18);

    for (int width : {640, 800, 1024}) {
        const ScreenLayout layout = layoutForCanvas(width);
        CardShopView view(nullptr, assets.root, 'D', layout, texts, text);
        CHECK(idle(key(view, game, SDLK_RETURN)));
        view.open(game);
        CHECK(idle(key(view, game, SDLK_LEFT)));
        const auto buyMode = key(view, game, SDLK_RETURN);
        CHECK(buyMode.kind == Kind::None && buyMode.sound == 2); // Left at Kaufen stays there
        // Isolate subsequent checks even if this header-navigation regression fails.
        view.open(game);
        CHECK(key(view, game, SDLK_RETURN).sound == 2);
        auto selected = [&](int index) {
            const auto action = key(view, game, SDLK_RETURN);
            CHECK(action.kind == Kind::Buy && action.value == offers[size_t(index)].type);
        };
        selected(0);
        CHECK(idle(click(view, game, layout, 15, 51)));
        CHECK(idle(click(view, game, layout, 15, 51))); // two singles must not buy
        CHECK(click(view, game, layout, 15, 51, 2).kind == Kind::Buy);
        CHECK(idle(key(view, game, SDLK_UP)));
        selected(0);
        int index = 0;
        while (index + 9 < int(offers.size())) {
            CHECK(idle(key(view, game, SDLK_DOWN)));
            index += 9;
            selected(index);
        }
        CHECK(idle(key(view, game, SDLK_DOWN)));
        selected(index); // no card in the same column of the next row
        CHECK(idle(key(view, game, SDLK_UP)));
        selected(index - 9);

        for (int i = 0; i < int(offers.size()); ++i) {
            const int x = 15 + (i % 9) * 60, y = 51 + (i / 9) * 74;
            CHECK(idle(click(view, game, layout, x, y)));
            selected(i);
            CHECK(idle(click(view, game, layout, x + 43, y + 62)));
            selected(i);
            CHECK(idle(click(view, game, layout, x + 44, y + 62)));
            selected(i); // the right edge is excluded
            CHECK(idle(click(view, game, layout, x, y + 63)));
            selected(i); // the bottom edge is excluded
        }
        CHECK(idle(key(view, game, SDLK_RIGHT)));
        selected(int(offers.size()) - 1);
        CHECK(click(view, game, layout, 300, 440, 2).kind == Kind::Buy); // double-click posts Enter anywhere
        CHECK(idle(key(view, game, SDLK_ESCAPE))); // returning to the header is silent
        CHECK(key(view, game, SDLK_ESCAPE).kind == Kind::Close);

        view.open(game);
        CHECK(idle(click(view, game, layout, -1, 10)));
        CHECK(idle(click(view, game, layout, 558, 10)));
        CHECK(idle(click(view, game, layout, 110, 10)));
        CHECK(idle(click(view, game, layout, 320, 10)));
        key(view, game, SDLK_RIGHT); // sell tab
        CHECK(key(view, game, SDLK_RETURN).sound == 2);
        CHECK(idle(key(view, game, SDLK_RIGHT)));
        CHECK(idle(key(view, game, SDLK_RIGHT)));
        const auto sale = key(view, game, SDLK_RETURN);
        CHECK(sale.kind == Kind::Sell && sale.value == 3); // third list item is hand slot 3
        CHECK(idle(click(view, game, layout, 135, 51)));
        CHECK(idle(click(view, game, layout, 135, 51)));
        CHECK(click(view, game, layout, 135, 51, 2).value == 3);
        CHECK(idle(click(view, game, layout, 20, 20, 1, SDL_BUTTON_RIGHT)));
        CHECK(key(view, game, SDLK_RETURN).sound == 2); // restored sell choice
        CHECK(key(view, game, SDLK_RETURN).kind == Kind::Sell);
        key(view, game, SDLK_BACKSPACE);
        key(view, game, SDLK_RIGHT); // Beenden
        CHECK(key(view, game, SDLK_RETURN).kind == Kind::Close);

        view.open(game);
        key(view, game, SDLK_RIGHT);
        key(view, game, SDLK_RIGHT);
        key(view, game, SDLK_RIGHT); // right wraps from Beenden to Kaufen
        CHECK(key(view, game, SDLK_RETURN).sound == 2);
        CHECK(key(view, game, SDLK_RETURN).kind == Kind::Buy);
        view.open(empty);
        key(view, empty, SDLK_RIGHT);
        CHECK(idle(key(view, empty, SDLK_RETURN))); // selling an empty hand is disabled
        key(view, empty, SDLK_RIGHT);
        CHECK(key(view, empty, SDLK_RETURN).kind == Kind::Close);
        view.close();
        CHECK(idle(key(view, game, SDLK_RETURN)));
    }

    // Round 101 (seg3:49E0 ff.): selling the last card returns to the header.
    {
        Game single(board, towns, 2, 2, 3, 1);
        arrive(single);
        const ScreenLayout layout640 = layoutForCanvas(640); // the view keeps a reference
        CardShopView view(nullptr, assets.root, 'D', layout640, texts, text);
        view.open(single);
        key(view, single, SDLK_RIGHT);                              // Verkaufen
        CHECK(key(view, single, SDLK_RETURN).sound == 2);           // sell mode
        const auto sale = key(view, single, SDLK_RETURN);
        CHECK(sale.kind == Kind::Sell && single.sellCard(sale.value) && single.handCount(0) == 0);
        view.afterSale(single);
        CHECK(idle(key(view, single, SDLK_RETURN)));                 // header: Verkaufen is disabled now
        CHECK(key(view, single, SDLK_ESCAPE).kind == Kind::Close);   // from the header Escape closes
    }
    return failures ? 1 : 0;
}
