// Forschungsbaum (Spoiler) und Planer fuer MAM und Pionier-Talente im Handbuch (22.11.2026, Marius).
// Daten: baeume-daten.js (erzeugt von Scripts/erzeuge_doku_baeume.py aus dem Code). Regeln wie im Spiel:
//   - Festplatten bis vor Welle n: je Welle Einsatz.FP.JeWelle, +KernAb75 wenn der Kern ueber 75 % bleibt, +Boss je zehnte Welle,
//     wahlweise der Bonus "Festplatten" nach jeder Bosswelle und im Shop gekaufte Festplatten
//   - ein Knoten braucht seine Eltern und Voraussetzungen, seine Freigabe-Welle und genug Festplatten; Gabelungen schliessen sich aus
//   - Talentpunkte: einer je geschaffter Bosswelle (Pionier.PunkteJeBoss), hoechstens Pionier.MaxPunkte; Stufen je Rolle der Reihe nach
// Der Plan steht in der Adresse (#plan=...), damit man Builds teilen kann.
(function () {
  'use strict';
  var D = window.TD_DATEN;
  var wurzel = document.getElementById('tdPlaner');
  if (!D || !wurzel) { return; }
  var Z = D.zahlen;
  var KW = 168, KH = 112;   // Rasterzelle des Baums in px (Knoten 150 x 92)

  var knoten = {}, baumVon = {}, gabelPartner = {};
  D.baeume.forEach(function (b) { b.knoten.forEach(function (k) { knoten[k.key] = k; baumVon[k.key] = b; }); });
  D.gabeln.forEach(function (g) { g.forEach(function (k) { gabelPartner[k] = g.filter(function (x) { return x !== k; }); }); });
  var kinder = {};
  Object.keys(knoten).forEach(function (k) { knoten[k].eltern.concat(knoten[k].braucht).forEach(function (e) { (kinder[e] = kinder[e] || []).push(k); }); });

  var S = { welle: 3, kern: true, bonus: false, gekauft: 0, endlos: false, bohrung: 0, reihe: [], talente: [0, 0, 0, 0], tab: D.baeume[0].key, wahl: null };

  function zwei(de, en) { return '<span class="de">' + de + '</span><span class="en">' + en + '</span>'; }
  function esc(t) { return String(t).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;'); }
  function zahl(n) { return String(Math.round(n * 10) / 10).replace('.', ','); }

  // ---------- Rechnen ----------
  function bossBis(n) { return Math.floor((n - 1) / Z.bossAlle); }
  function festplattenVerdient() {
    var fp = 0;
    for (var i = 1; i < S.welle; i++) {
      fp += Z.fpJeWelle + (S.kern ? Z.fpKern75 : 0);
      if (i % Z.bossAlle === 0) { fp += Z.fpBoss + (S.bonus ? Z.bonusFestplatten : 0); }
    }
    return fp + S.gekauft;
  }
  function shopKosten(n) { var c = 0; for (var i = 0; i < n; i++) { c += Math.round(Z.festplattePreis * (1 + Z.festplatteAufschlag * i)); } return c; }
  function ausgegeben() { return S.reihe.reduce(function (s, k) { return s + knoten[k].fp; }, 0); }
  function hat(k) { return S.reihe.indexOf(k) >= 0; }
  function zustand(k) {
    var K = knoten[k];
    if (hat(k)) { return 'erforscht'; }
    if ((gabelPartner[k] || []).some(hat)) { return 'aus'; }
    if (K.eltern.concat(K.braucht).some(function (e) { return !hat(e); })) { return 'gesperrt'; }
    if (K.welle > S.welle) { return 'frueh'; }
    if (K.fp > festplattenVerdient() - ausgegeben()) { return 'teuer'; }
    return 'frei';
  }
  function punkteDa() { return Math.min(Z.punkteMax, bossBis(S.welle) * Z.punkteJeBoss + S.bohrung); }
  function punkteWeg() { return S.talente.reduce(function (a, b) { return a + b; }, 0); }

  // Plan an Welle und Festplatten anpassen: zu frueh, ohne Voraussetzung oder ueber dem Budget faellt von hinten weg
  function bereinige() {
    var geaendert = true;
    while (geaendert) {
      geaendert = false;
      for (var i = 0; i < S.reihe.length; i++) {
        var K = knoten[S.reihe[i]];
        var fehlt = K.welle > S.welle || K.eltern.concat(K.braucht).some(function (e) { return S.reihe.indexOf(e) < 0; });
        if (fehlt) { S.reihe.splice(i, 1); geaendert = true; break; }
      }
    }
    while (S.reihe.length && ausgegeben() > festplattenVerdient()) { S.reihe.pop(); }
    for (var z = 3; punkteWeg() > punkteDa() && z >= 0; z--) { while (S.talente[z] > 0 && punkteWeg() > punkteDa()) { S.talente[z]--; } }
  }

  // ---------- Adresse ----------
  function speichere() {
    var p = ['w' + S.welle, 'k' + (S.kern ? 1 : 0), 'b' + (S.bonus ? 1 : 0), 'g' + S.gekauft, 'e' + (S.endlos ? 1 : 0), 'o' + S.bohrung,
      't' + S.talente.join(''), 'r' + S.reihe.join(','), 'a' + S.tab];
    try { history.replaceState(null, '', '#plan=' + p.join(';')); } catch (e) {}
  }
  function lade() {
    var m = /#plan=([^#]*)/.exec(location.hash);
    if (!m) { return false; }
    decodeURIComponent(m[1]).split(';').forEach(function (t) {
      var c = t.charAt(0), v = t.slice(1);
      if (c === 'w') { S.welle = Math.max(1, Math.min(150, parseInt(v, 10) || 3)); }
      if (c === 'k') { S.kern = v === '1'; }
      if (c === 'b') { S.bonus = v === '1'; }
      if (c === 'g') { S.gekauft = Math.max(0, Math.min(60, parseInt(v, 10) || 0)); }
      if (c === 'e') { S.endlos = v === '1'; }
      if (c === 'o') { S.bohrung = Math.max(0, Math.min(15, parseInt(v, 10) || 0)); }
      if (c === 't') { S.talente = v.split('').slice(0, 4).map(function (x) { return Math.max(0, Math.min(5, parseInt(x, 10) || 0)); }); while (S.talente.length < 4) { S.talente.push(0); } }
      if (c === 'r') { S.reihe = v ? v.split(',').filter(function (k) { return knoten[k]; }) : []; }
      if (c === 'a') { S.tab = v; }
    });
    if (S.welle > Z.wellenEinsatz) { S.endlos = true; }
    return true;
  }

  // ---------- Zeichnen ----------
  var el = {
    welle: wurzel.querySelector('#tdWelle'), welleZahl: wurzel.querySelector('#tdWelleZahl'), kern: wurzel.querySelector('#tdKern'),
    bonus: wurzel.querySelector('#tdBonus'), gekauft: wurzel.querySelector('#tdGekauft'), endlos: wurzel.querySelector('#tdEndlos'),
    bohrung: wurzel.querySelector('#tdBohrung'), stand: wurzel.querySelector('#tdStand'), reiter: wurzel.querySelector('#tdReiter'),
    flaeche: wurzel.querySelector('#tdFlaeche'), info: wurzel.querySelector('#tdInfo')
  };

  var ZUSTAND = {
    erforscht: ['Erforscht', 'Researched'], frei: ['Jetzt erforschbar', 'Available now'], teuer: ['Zu wenige Festplatten', 'Not enough Hard Drives'],
    frueh: ['Erst ab Welle', 'Only from wave'], gesperrt: ['Voraussetzung fehlt', 'Missing prerequisite'], aus: ['Ausgeschlossen: andere Seite gewählt', 'Excluded: other side chosen']
  };

  function stand() {
    var verdient = festplattenVerdient(), weg = ausgegeben();
    el.stand.innerHTML =
      '<div><b>' + zwei('Festplatten', 'Hard Drives') + '</b> <span class="zahl">' + (verdient - weg) + '</span> ' + zwei('frei', 'left') +
      ' <small>' + zwei('von ' + verdient + ' (davon ' + S.gekauft + ' gekauft für ' + shopKosten(S.gekauft) + ' Coupons), ausgegeben ' + weg,
        'of ' + verdient + ' (' + S.gekauft + ' bought for ' + shopKosten(S.gekauft) + ' Coupons), spent ' + weg) + '</small></div>' +
      '<div><b>' + zwei('Talentpunkte', 'Talent Points') + '</b> <span class="zahl">' + (punkteDa() - punkteWeg()) + '</span> ' + zwei('frei', 'left') +
      ' <small>' + zwei('von ' + punkteDa() + ' (' + bossBis(S.welle) + ' Bosswellen geschafft, höchstens ' + Z.punkteMax + ')',
        'of ' + punkteDa() + ' (' + bossBis(S.welle) + ' boss waves cleared, at most ' + Z.punkteMax + ')') + '</small></div>' +
      '<div><b>' + zwei('Erforscht', 'Researched') + '</b> <span class="zahl">' + S.reihe.length + '</span> ' + zwei('von ' + Object.keys(knoten).length + ' Knoten', 'of ' + Object.keys(knoten).length + ' nodes') + '</div>';
  }

  function reiter() {
    var tabs = D.baeume.map(function (b) { return { key: b.key, de: b.de.name, en: b.en.name }; });
    tabs.push({ key: 'Pionier', de: 'Pionier', en: 'Pioneer' });
    el.reiter.innerHTML = tabs.map(function (t) {
      var n = t.key === 'Pionier' ? punkteWeg() : S.reihe.filter(function (k) { return baumVon[k].key === t.key; }).length;
      return '<button type="button" role="tab" data-tab="' + t.key + '" aria-selected="' + (S.tab === t.key) + '">' + zwei(esc(t.de), esc(t.en)) +
        (n ? ' <small>' + n + '</small>' : '') + '</button>';
    }).join('');
  }

  function baum(b) {
    var maxX = 0, maxY = 0;
    b.knoten.forEach(function (k) { maxX = Math.max(maxX, k.x); maxY = Math.max(maxY, k.y); });
    var B = (maxX + 1) * KW, H = (maxY + 1) * KH;
    var linien = '';
    b.knoten.forEach(function (k) {
      k.eltern.forEach(function (e) {
        var p = knoten[e];
        if (!p || baumVon[e] !== b) { return; }
        // wie im Spiel: waagerecht in der Zeile des Elternknotens, dann senkrecht bis an die Oberkante des Kindes (nicht durch Knoten hindurch)
        var x2 = k.x * KW + 75, y2 = k.y * KH, an = hat(e) ? ' an' : '', d;
        if (p.x === k.x) { d = 'M' + x2 + ' ' + (p.y * KH + 92) + ' V' + y2; }
        else { d = 'M' + (p.x * KW + (k.x > p.x ? 150 : 0)) + ' ' + (p.y * KH + 46) + ' H' + x2 + ' V' + y2; }
        linien += '<path class="linie' + an + '" d="' + d + '"/>';
      });
    });
    var karten = b.knoten.map(function (k) {
      var z = zustand(k.key), gabel = gabelPartner[k.key] ? ' <i class="gabel">' + zwei('Gabel', 'Branch') + '</i>' : '';
      var bild = k.bild ? '<img src="img/' + k.bild + '.png" alt="" loading="lazy">' : '';
      return '<button type="button" class="knoten z-' + z + (S.wahl === k.key ? ' gewaehlt' : '') + '" data-k="' + k.key + '" style="left:' + (k.x * KW) + 'px;top:' + (k.y * KH) + 'px"' +
        ' title="' + esc(k.de.name) + '">' + bild + '<b>' + zwei(esc(k.de.name), esc(k.en.name)) + '</b>' +
        '<small>' + zwei((k.welle ? 'ab W' + k.welle + ' · ' : '') + k.fp + ' FP', (k.welle ? 'from W' + k.welle + ' · ' : '') + k.fp + ' HD') + gabel + '</small></button>';
    }).join('');
    return '<p class="baumtext">' + zwei(esc(b.de.text), esc(b.en.text)) + '</p>' +
      '<div class="scroll"><div class="feld" style="width:' + B + 'px;height:' + H + 'px"><svg width="' + B + '" height="' + H + '" aria-hidden="true">' + linien + '</svg>' + karten + '</div></div>';
  }

  function namenListe(liste) {
    return liste.map(function (k) {
      var K = knoten[k], b = baumVon[k];
      return '<span class="' + (hat(k) ? 'ok' : 'fehlt') + '">' + zwei(esc(b.de.name + ' > ' + K.de.name), esc(b.en.name + ' > ' + K.en.name)) + '</span>';
    }).join(', ');
  }

  function info() {
    if (S.tab === 'Pionier') { el.info.hidden = true; return; }
    el.info.hidden = false;
    var K = S.wahl && knoten[S.wahl] && baumVon[S.wahl].key === S.tab ? knoten[S.wahl] : null;
    if (!K) {
      el.info.innerHTML = '<p class="leise">' + zwei('Klicke einen Knoten: erforschbare Knoten werden erforscht, erforschte wieder entfernt (mit allem, was darauf aufbaut). Die Zahlen gelten für den Einsatz.',
        'Click a node: available nodes get researched, researched ones are removed again (with everything built on them). Numbers are for an operation.') + '</p>';
      return;
    }
    var z = zustand(K.key), grund = ZUSTAND[z];
    var zeilen = '<h4>' + zwei(esc(K.de.name), esc(K.en.name)) + ' <span class="zust z-' + z + '">' + zwei(grund[0] + (z === 'frueh' ? ' ' + K.welle : ''), grund[1] + (z === 'frueh' ? ' ' + K.welle : '')) + '</span></h4>';
    if (K.de.kurz) { zeilen += '<p class="kurz">' + zwei(esc(K.de.kurz), esc(K.en.kurz || '')) + '</p>'; }
    zeilen += '<p>' + zwei(esc(K.de.text), esc(K.en.text)) + '</p><ul class="fakten">' +
      '<li>' + zwei('Ab Welle', 'From wave') + ' <b>' + K.welle + '</b> · ' + zwei('Kosten', 'Cost') + ' <b>' + K.fp + '</b> ' + zwei('Festplatten', 'Hard Drives') + '</li>';
    var vor = K.eltern.concat(K.braucht);
    if (vor.length) { zeilen += '<li>' + zwei('Braucht', 'Requires') + ': ' + namenListe(vor) + '</li>'; }
    if (gabelPartner[K.key]) { zeilen += '<li>' + zwei('Gabelung, schließt aus', 'Branch, excludes') + ': ' + namenListe(gabelPartner[K.key]) + '</li>'; }
    if (kinder[K.key]) { zeilen += '<li>' + zwei('Führt zu', 'Leads to') + ': ' + namenListe(kinder[K.key]) + '</li>'; }
    el.info.innerHTML = zeilen + '</ul>';
  }

  function pionier() {
    var de = D.talente.de, en = D.talente.en;
    return '<p class="baumtext">' + zwei('Talentpunkte gibt es für jede geschaffte Bosswelle (Welle 10, 20, 30 …) und für Erfolge in der Bohrung, höchstens ' + Z.punkteMax + '. Jede Rolle hat fünf Stufen, die der Reihe nach kommen. Klicke eine Stufe, um bis dorthin zu lernen; ein Klick auf eine gelernte Stufe nimmt sie und alles darüber zurück.',
      'Talent Points come from every boss wave you clear (wave 10, 20, 30 …) and from drilling successes, at most ' + Z.punkteMax + '. Each role has five levels in order. Click a level to learn up to it; clicking a learned level removes it and everything above.') + '</p>' +
      '<div class="rollen">' + de.map(function (z, i) {
        var stufen = z.stufen.map(function (t, s) {
          var gelernt = s < S.talente[i], moeglich = !gelernt && (s + 1 - S.talente[i]) <= (punkteDa() - punkteWeg());
          return '<button type="button" class="stufe ' + (gelernt ? 'z-erforscht' : moeglich ? 'z-frei' : 'z-teuer') + '" data-rolle="' + i + '" data-stufe="' + (s + 1) + '">' +
            '<i>' + (s + 1) + '</i>' + zwei(esc(t), esc(en[i].stufen[s])) + '</button>';
        }).join('');
        return '<div class="rolle"><h4>' + zwei(esc(z.name), esc(en[i].name)) + ' <small>' + S.talente[i] + '/5</small></h4><p class="leise">' + zwei(esc(z.unter), esc(en[i].unter)) + '</p>' + stufen + '</div>';
      }).join('') + '</div>';
  }

  function zeichne() {
    bereinige();
    var max = S.endlos ? 150 : Z.wellenEinsatz;
    el.welle.max = max;
    if (S.welle > max) { S.welle = max; }
    el.welle.value = S.welle;
    el.welleZahl.textContent = S.welle;
    el.kern.checked = S.kern; el.bonus.checked = S.bonus; el.gekauft.value = S.gekauft; el.endlos.checked = S.endlos; el.bohrung.value = S.bohrung;
    stand();
    reiter();
    if (S.tab === 'Pionier') { el.flaeche.innerHTML = pionier(); }
    else {
      var b = D.baeume.filter(function (x) { return x.key === S.tab; })[0] || D.baeume[0];
      S.tab = b.key;
      el.flaeche.innerHTML = baum(b);
    }
    info();
    speichere();
  }

  // ---------- Bedienung ----------
  function entferne(k) {
    var weg = [k], i = 0;
    while (i < weg.length) { (kinder[weg[i]] || []).forEach(function (c) { if (weg.indexOf(c) < 0 && hat(c)) { weg.push(c); } }); i++; }
    S.reihe = S.reihe.filter(function (x) { return weg.indexOf(x) < 0; });
  }
  el.flaeche.addEventListener('click', function (ev) {
    var t = ev.target.closest('button');
    if (!t) { return; }
    if (t.dataset.k) {
      var k = t.dataset.k, z = zustand(k);
      if (z === 'erforscht') { entferne(k); } else if (z === 'frei') { S.reihe.push(k); }
      S.wahl = k;
    } else if (t.dataset.rolle) {
      var r = parseInt(t.dataset.rolle, 10), s = parseInt(t.dataset.stufe, 10);
      if (s <= S.talente[r]) { S.talente[r] = s - 1; }
      else if (s - S.talente[r] <= punkteDa() - punkteWeg()) { S.talente[r] = s; }
    }
    zeichne();
  });
  el.reiter.addEventListener('click', function (ev) { var t = ev.target.closest('button'); if (t) { S.tab = t.dataset.tab; zeichne(); } });
  el.welle.addEventListener('input', function () { S.welle = parseInt(el.welle.value, 10); zeichne(); });
  el.kern.addEventListener('change', function () { S.kern = el.kern.checked; zeichne(); });
  el.bonus.addEventListener('change', function () { S.bonus = el.bonus.checked; zeichne(); });
  el.endlos.addEventListener('change', function () { S.endlos = el.endlos.checked; zeichne(); });
  el.gekauft.addEventListener('input', function () { S.gekauft = Math.max(0, Math.min(60, parseInt(el.gekauft.value, 10) || 0)); zeichne(); });
  el.bohrung.addEventListener('input', function () { S.bohrung = Math.max(0, Math.min(15, parseInt(el.bohrung.value, 10) || 0)); zeichne(); });
  wurzel.querySelector('#tdLeeren').addEventListener('click', function () { S.reihe = []; S.talente = [0, 0, 0, 0]; S.wahl = null; zeichne(); });
  wurzel.querySelector('#tdTeilen').addEventListener('click', function (ev) {
    var knopf = ev.currentTarget;
    function fertig() { knopf.classList.add('ok'); setTimeout(function () { knopf.classList.remove('ok'); }, 1500); }
    try { navigator.clipboard.writeText(location.href).then(fertig, fertig); } catch (e) { fertig(); }
  });

  var spoiler = document.getElementById('tdSpoiler');
  function oeffne() { wurzel.hidden = false; spoiler.setAttribute('aria-expanded', 'true'); zeichne(); }
  spoiler.addEventListener('click', function () {
    if (wurzel.hidden) { oeffne(); } else { wurzel.hidden = true; spoiler.setAttribute('aria-expanded', 'false'); }
  });
  if (lade()) { oeffne(); }
})();
