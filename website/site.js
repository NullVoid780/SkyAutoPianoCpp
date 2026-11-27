const root = document.documentElement;
const darkQuery = window.matchMedia("(prefers-color-scheme: dark)");

function activeTheme() {
  return root.dataset.theme || (darkQuery.matches ? "dark" : "light");
}

function applyTheme() {
  const theme = activeTheme();
  root.dataset.activeTheme = theme;
  document.querySelectorAll("picture source[data-dark]").forEach((source) => {
    source.media = theme === "dark" ? "all" : "not all";
  });
}

document.querySelectorAll("picture source[media*='dark']").forEach((source) => {
  source.dataset.dark = "";
});
applyTheme();
darkQuery.addEventListener("change", applyTheme);

document.querySelectorAll("[data-theme-toggle]").forEach((button) => {
  button.addEventListener("click", () => {
    const next = activeTheme() === "dark" ? "light" : "dark";
    root.dataset.theme = next;
    localStorage.setItem("theme", next);
    applyTheme();
  });
});

function flash(button, text) {
  const label = button.querySelector("[data-label]") || button;
  const original = label.dataset.original || label.textContent;
  label.dataset.original = original;
  label.textContent = text;
  setTimeout(() => {
    label.textContent = original;
  }, 1600);
}

async function copyText(text) {
  try {
    await navigator.clipboard.writeText(text);
  } catch {
    const area = document.createElement("textarea");
    area.value = text;
    area.setAttribute("readonly", "");
    area.style.position = "fixed";
    area.style.opacity = "0";
    document.body.append(area);
    area.select();
    const copied = document.execCommand("copy");
    area.remove();
    if (!copied) {
      throw new Error("copy failed");
    }
  }
}

document.querySelectorAll(".code:not(.code-plain)").forEach((block) => {
  const button = document.createElement("button");
  button.type = "button";
  button.className = "copy";
  button.textContent = "Copy";
  button.addEventListener("click", async () => {
    try {
      await copyText(block.querySelector("code").innerText);
      flash(button, "Copied");
    } catch {
      flash(button, "Press Ctrl+C");
    }
  });
  block.append(button);
});

const skipInMarkdown = ".page-actions, .crumbs, .pager, .docs-footer, .copy, script, style, svg, button";

function wrapMarkdown(mark, text) {
  const inner = text.trim();
  if (!inner) {
    return text;
  }
  const before = /^\s/.test(text) ? " " : "";
  const after = /\s$/.test(text) ? " " : "";
  return before + mark + inner + mark + after;
}

function inlineMarkdown(node) {
  let out = "";
  node.childNodes.forEach((child) => {
    if (child.nodeType === Node.TEXT_NODE) {
      out += child.textContent.replace(/\s+/g, " ");
      return;
    }
    if (child.nodeType !== Node.ELEMENT_NODE || child.matches(skipInMarkdown) || child.matches("ul, ol")) {
      return;
    }
    const inner = inlineMarkdown(child);
    switch (child.tagName) {
      case "STRONG":
      case "B":
        out += wrapMarkdown("**", inner);
        break;
      case "EM":
      case "I":
        out += wrapMarkdown("*", inner);
        break;
      case "CODE":
        out += "`" + child.textContent + "`";
        break;
      case "KBD":
        out += "<kbd>" + child.textContent + "</kbd>";
        break;
      case "A":
        out += "[" + inner.trim() + "](" + child.href + ")";
        break;
      case "BR":
        out += "  \n";
        break;
      case "IMG":
        out += "![" + child.alt + "](" + child.src + ")";
        break;
      default:
        out += child.classList.contains("path") ? wrapMarkdown("**", inner) : inner;
    }
  });
  return out;
}

function indentMarkdown(text, spaces) {
  const pad = " ".repeat(spaces);
  return text
    .split("\n")
    .map((line) => (line ? pad + line : line))
    .join("\n");
}

function itemMarkdown(item, marker) {
  const text = inlineMarkdown(item).trim();
  const nested = [...item.children]
    .filter((child) => child.matches("ul, ol"))
    .map((list) => indentMarkdown(listMarkdown(list), marker.length + 1));
  return [marker + " " + text, ...nested].join("\n");
}

function listMarkdown(list) {
  const ordered = list.tagName === "OL";
  return [...list.children]
    .filter((child) => child.tagName === "LI")
    .map((item, index) => itemMarkdown(item, ordered ? index + 1 + "." : "-"))
    .join("\n");
}

function tableMarkdown(table) {
  const rows = [...table.querySelectorAll("tr")].map((row) =>
    [...row.children].map((cell) => inlineMarkdown(cell).trim().replace(/\|/g, "\\|")),
  );
  if (!rows.length) {
    return "";
  }
  const line = (cells) => "| " + cells.join(" | ") + " |";
  const divider = line(rows[0].map(() => "---"));
  return [line(rows[0]), divider, ...rows.slice(1).map(line)].join("\n");
}

function blockMarkdown(node, out) {
  for (const child of node.children) {
    if (child.matches(skipInMarkdown)) {
      continue;
    }
    const tag = child.tagName;
    if (/^H[1-6]$/.test(tag)) {
      out.push("#".repeat(Number(tag[1])) + " " + inlineMarkdown(child).trim());
    } else if (tag === "P") {
      const text = inlineMarkdown(child).trim();
      if (!text) {
        continue;
      }
      if (child.classList.contains("tip")) {
        out.push("> **Tip:** " + text);
      } else if (child.classList.contains("warn")) {
        out.push("> **Warning:** " + text);
      } else {
        out.push(text);
      }
    } else if (tag === "UL" || tag === "OL") {
      out.push(listMarkdown(child));
    } else if (tag === "DL") {
      const items = [...child.querySelectorAll("dt")].map((term) => {
        const detail = term.nextElementSibling;
        const item = document.createElement("li");
        item.innerHTML = "<strong>" + term.innerHTML + ":</strong> ";
        if (detail) {
          item.append(...[...detail.childNodes].map((part) => part.cloneNode(true)));
        }
        return itemMarkdown(item, "-");
      });
      out.push(items.join("\n"));
    } else if (child.classList.contains("code") || tag === "PRE") {
      const code = child.querySelector("code") || child;
      out.push("```\n" + code.textContent.replace(/\n+$/, "") + "\n```");
    } else if (child.classList.contains("table") || tag === "TABLE") {
      const table = tag === "TABLE" ? child : child.querySelector("table");
      if (table) {
        out.push(tableMarkdown(table));
      }
    } else if (tag === "FIGURE") {
      const image = child.querySelector("img");
      if (image) {
        out.push("![" + image.alt + "](" + image.src + ")");
      }
      const caption = child.querySelector("figcaption");
      if (caption) {
        out.push(inlineMarkdown(caption).trim());
      }
    } else if (tag === "SUMMARY") {
      out.push("**" + inlineMarkdown(child).trim() + "**");
    } else if (child.children.length && !child.matches("a, span, strong, code")) {
      blockMarkdown(child, out);
    } else {
      const text = inlineMarkdown(child).trim();
      if (text) {
        out.push(text);
      }
    }
  }
  return out;
}

function pageMarkdown() {
  const article = document.querySelector(".docs-article");
  return article ? blockMarkdown(article, []).join("\n\n") + "\n" : "";
}

document.querySelectorAll("[data-copy-markdown]").forEach((button) => {
  button.addEventListener("click", async () => {
    try {
      await copyText(pageMarkdown());
      flash(button, "Copied");
    } catch {
      flash(button, "Couldn't copy");
    }
  });
});

const sidebar = document.querySelector(".docs-sidebar");
const toc = document.querySelector(".docs-toc");

function setOpen(panel, trigger, open) {
  if (!panel) {
    return;
  }
  panel.classList.toggle("open", open);
  document.querySelectorAll(trigger).forEach((button) => button.setAttribute("aria-expanded", String(open)));
}

document.querySelectorAll("[data-menu]").forEach((button) => {
  button.addEventListener("click", () => {
    setOpen(toc, "[data-toc-toggle]", false);
    setOpen(sidebar, "[data-menu]", !sidebar.classList.contains("open"));
  });
});

document.querySelectorAll("[data-toc-toggle]").forEach((button) => {
  button.addEventListener("click", () => {
    setOpen(sidebar, "[data-menu]", false);
    setOpen(toc, "[data-toc-toggle]", !toc.classList.contains("open"));
  });
});

document.querySelectorAll(".docs-sidebar a, .docs-toc a").forEach((link) => {
  link.addEventListener("click", () => {
    setOpen(sidebar, "[data-menu]", false);
    setOpen(toc, "[data-toc-toggle]", false);
  });
});

const tocLinks = [...document.querySelectorAll(".toc-list a")];

function updateToc() {
  let current = tocLinks[0];
  for (const link of tocLinks) {
    const target = document.getElementById(link.hash.slice(1));
    if (target && target.getBoundingClientRect().top < 120) {
      current = link;
    }
  }
  tocLinks.forEach((link) => link.classList.toggle("current", link === current));
}

if (tocLinks.length) {
  window.addEventListener("scroll", updateToc, { passive: true });
  updateToc();
}

const pages = [
  { url: "index.html", title: "Home" },
  { url: "install.html", title: "Install guide" },
  { url: "guide.html", title: "User guide" },
  { url: "themes.html", title: "Themes" },
  { url: "build.html", title: "Build from source" },
  { url: "contributing.html", title: "Contributing" },
];

const inlineTags = /^(A|B|STRONG|I|EM|CODE|KBD|SPAN|MARK|SMALL|SUB|SUP|ABBR)$/;

function rawText(node) {
  let text = "";
  node.childNodes.forEach((child) => {
    if (child.nodeType === 3) {
      text += child.textContent;
    } else if (child.nodeType === 1 && !child.matches("svg, button, script, style, .copy")) {
      if (child.tagName === "BR") {
        text += " ";
      } else if (inlineTags.test(child.tagName)) {
        text += rawText(child);
      } else {
        text += " " + rawText(child) + " ";
      }
    }
  });
  return text;
}

function plainText(node) {
  return rawText(node).replace(/\s+/g, " ").trim();
}

function sectionsOf(doc, url, pageTitle) {
  const root = doc.querySelector(".docs-article") || doc.querySelector("main") || doc.body;
  const entries = [];
  root.querySelectorAll("h1, h2, h3, summary").forEach((heading) => {
    if (heading.closest(".page-actions, .crumbs, .pager, .docs-footer")) {
      return;
    }
    const anchor = heading.id || (heading.closest("[id]:not(main)") || {}).id || "";
    let text = "";
    if (heading.tagName === "SUMMARY") {
      [...heading.parentElement.children].forEach((part) => {
        if (part !== heading) {
          text += " " + plainText(part);
        }
      });
    } else {
      let node = heading.nextElementSibling;
      while (node && !node.matches("h1, h2, h3") && text.length < 600) {
        text += " " + plainText(node);
        node = node.nextElementSibling;
      }
    }
    if (!text && heading.tagName === "H1") {
      const lead = doc.querySelector(".docs-lead, .home-lead");
      text = lead ? plainText(lead) : "";
    }
    const title = plainText(heading);
    if (!title) {
      return;
    }
    entries.push({
      page: pageTitle,
      title,
      url: heading.tagName === "H1" || !anchor ? url : url + "#" + anchor,
      text: text.replace(/\s+/g, " ").trim(),
    });
  });
  return entries;
}

const framedForIndex = window.parent !== window && new URLSearchParams(location.search).has("search-index");
if (framedForIndex) {
  const here = location.pathname.split("/").pop() || "index.html";
  const page = pages.find((entry) => entry.url === here);
  window.parent.postMessage({ type: "sky-search-index", entries: page ? sectionsOf(document, page.url, page.title) : [] }, "*");
}

function pageFromFetch(page) {
  return fetch(page.url)
    .then((response) => {
      if (!response.ok) {
        throw new Error(String(response.status));
      }
      return response.text();
    })
    .then((html) => sectionsOf(new DOMParser().parseFromString(html, "text/html"), page.url, page.title));
}

function pageFromFrame(page) {
  return new Promise((resolve) => {
    const frame = document.createElement("iframe");
    frame.hidden = true;
    frame.setAttribute("aria-hidden", "true");
    frame.tabIndex = -1;
    const finish = (entries) => {
      window.removeEventListener("message", listen);
      clearTimeout(timer);
      frame.remove();
      resolve(entries);
    };
    const listen = (event) => {
      if (event.source === frame.contentWindow && event.data && event.data.type === "sky-search-index") {
        finish(Array.isArray(event.data.entries) ? event.data.entries : []);
      }
    };
    const timer = setTimeout(() => finish([]), 8000);
    window.addEventListener("message", listen);
    frame.src = page.url + "?search-index";
    document.body.append(frame);
  });
}

let index = null;
let indexing = null;

function buildIndex() {
  if (index) {
    return Promise.resolve(index);
  }
  if (!indexing) {
    const here = location.pathname.split("/").pop() || "index.html";
    const current = pages.find((page) => page.url === here);
    const found = current ? sectionsOf(document, here, current.title) : [];
    const others = pages.filter((page) => page !== current);
    indexing = Promise.all(others.map((page) => pageFromFetch(page).catch(() => pageFromFrame(page)))).then((lists) => {
      index = found.concat(...lists);
      return index;
    });
  }
  return indexing;
}

function escapeHtml(text) {
  return text.replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" })[c]);
}

function highlight(text, words) {
  let html = escapeHtml(text);
  for (const word of words) {
    html = html.replace(new RegExp("(" + word.replace(/[.*+?^${}()|[\]\\]/g, "\\$&") + ")", "gi"), "<mark>$1</mark>");
  }
  return html;
}

function snippet(text, word) {
  const at = text.toLowerCase().indexOf(word);
  if (at < 0) {
    return text.slice(0, 110);
  }
  const start = Math.max(0, at - 40);
  return (start > 0 ? "…" : "") + text.slice(start, start + 120) + (start + 120 < text.length ? "…" : "");
}

const overlay = document.createElement("div");
overlay.className = "search-overlay";
overlay.hidden = true;
overlay.innerHTML =
  '<div class="search-panel" role="dialog" aria-modal="true" aria-label="Search the site">' +
  '<label class="search-field"><svg class="icon" viewBox="0 0 16 16" aria-hidden="true"><path d="M11.742 10.344a6.5 6.5 0 1 0-1.397 1.398h-.001q.044.06.098.115l3.85 3.85a1 1 0 0 0 1.415-1.414l-3.85-3.85a1 1 0 0 0-.115-.1zM12 6.5a5.5 5.5 0 1 1-11 0 5.5 5.5 0 0 1 11 0"/></svg>' +
  '<input type="search" placeholder="Search everything" autocomplete="off" spellcheck="false"></label>' +
  '<ul class="search-results"></ul>' +
  '<div class="search-hint"><span><kbd>↑</kbd> <kbd>↓</kbd> to move</span><span><kbd>Enter</kbd> to open</span><span><kbd>Esc</kbd> to close</span></div>' +
  "</div>";
document.body.append(overlay);
const input = overlay.querySelector("input");
const results = overlay.querySelector(".search-results");
let selected = 0;

function render(query) {
  const words = query.toLowerCase().split(/\s+/).filter(Boolean);
  if (!words.length) {
    results.innerHTML = '<li class="search-empty">Type to search every page.</li>';
    return;
  }
  if (!index) {
    results.innerHTML = '<li class="search-empty">Searching…</li>';
    return;
  }
  const scored = index
    .map((entry) => {
      const title = entry.title.toLowerCase();
      const text = entry.text.toLowerCase();
      let score = 0;
      for (const word of words) {
        if (title.includes(word)) {
          score += 3;
        } else if (text.includes(word)) {
          score += 1;
        } else {
          return null;
        }
      }
      return { entry, score };
    })
    .filter(Boolean)
    .sort((a, b) => b.score - a.score)
    .slice(0, 12);
  if (!scored.length) {
    results.innerHTML = '<li class="search-empty">Nothing found for “' + escapeHtml(query) + "”.</li>";
    return;
  }
  selected = 0;
  results.innerHTML = scored
    .map(
      ({ entry }, i) =>
        '<li class="' + (i === 0 ? "selected" : "") + '"><a href="' + entry.url + '">' +
        highlight(entry.title, words) +
        "<span>" + escapeHtml(entry.page) + " · " + highlight(snippet(entry.text, words[0]), words) + "</span></a></li>",
    )
    .join("");
}

function moveSelection(step) {
  const items = [...results.querySelectorAll("li:not(.search-empty)")];
  if (!items.length) {
    return;
  }
  items[selected].classList.remove("selected");
  selected = (selected + step + items.length) % items.length;
  items[selected].classList.add("selected");
  items[selected].scrollIntoView({ block: "nearest" });
}

async function openSearch() {
  overlay.hidden = false;
  input.value = "";
  render("");
  input.focus();
  await buildIndex();
  render(input.value);
}

function closeSearch() {
  overlay.hidden = true;
}

document.querySelectorAll("[data-search]").forEach((button) => {
  button.addEventListener("click", openSearch);
  button.addEventListener("pointerenter", buildIndex, { once: true });
  button.addEventListener("focus", buildIndex, { once: true });
});
input.addEventListener("input", () => render(input.value));
overlay.addEventListener("click", (event) => {
  if (event.target === overlay) {
    closeSearch();
  }
});
results.addEventListener("click", (event) => {
  if (event.target.closest("a")) {
    closeSearch();
  }
});

document.addEventListener("keydown", (event) => {
  const typing = /^(INPUT|TEXTAREA)$/.test(document.activeElement.tagName);
  if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "k") {
    event.preventDefault();
    overlay.hidden ? openSearch() : closeSearch();
    return;
  }
  if (event.key === "/" && !typing && overlay.hidden) {
    event.preventDefault();
    openSearch();
    return;
  }
  if (overlay.hidden) {
    if (event.key === "Escape") {
      setOpen(sidebar, "[data-menu]", false);
      setOpen(toc, "[data-toc-toggle]", false);
    }
    return;
  }
  if (event.key === "Escape") {
    closeSearch();
  } else if (event.key === "ArrowDown") {
    event.preventDefault();
    moveSelection(1);
  } else if (event.key === "ArrowUp") {
    event.preventDefault();
    moveSelection(-1);
  } else if (event.key === "Enter") {
    const link = results.querySelector("li.selected a");
    if (link) {
      event.preventDefault();
      closeSearch();
      location.href = link.href;
    }
  }
});

const revealed = document.querySelectorAll(".reveal");
if (revealed.length) {
  document.querySelectorAll(".reveal-group").forEach((group) => {
    [...group.querySelectorAll(":scope > .reveal")].forEach((item, i) => {
      item.style.setProperty("--delay", i * 70 + "ms");
    });
  });
  document.querySelectorAll(".row").forEach((row) => {
    const shot = row.querySelector(".shot.reveal");
    if (shot) {
      shot.style.setProperty("--delay", "120ms");
    }
  });
  if ("IntersectionObserver" in window) {
    const observer = new IntersectionObserver(
      (entries) => {
        entries.forEach((entry) => {
          if (entry.isIntersecting) {
            entry.target.classList.add("visible");
            observer.unobserve(entry.target);
          }
        });
      },
      { threshold: 0.12, rootMargin: "0px 0px -40px 0px" },
    );
    revealed.forEach((item) => observer.observe(item));
  } else {
    revealed.forEach((item) => item.classList.add("visible"));
  }
}
